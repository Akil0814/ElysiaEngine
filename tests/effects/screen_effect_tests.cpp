#define SDL_MAIN_HANDLED
#include "engine/effects/screen/screen_effect_runtime.h"
#include "engine/effects/effect_service.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/core/time.h"
#include "engine/scene/scene_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/texture/texture_loader.h"
#include "engine/ui/core/ui_element.h"
#include "engine/loading/content_runtime_cleanup.h"
#include "tests/support/test_assertions.h"
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace elysia;
using namespace elysia::effects;
using elysia::tests::require;

namespace
{
struct Fixture
{
    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
    Fixture()
    {
        require(SDL_Init(0), "SDL initialization");
        surface = SDL_CreateSurface(80, 60, SDL_PIXELFORMAT_RGBA32);
        require(surface != nullptr, "surface creation");
        renderer = SDL_CreateSoftwareRenderer(surface);
        require(renderer != nullptr, "software renderer creation");
    }
    ~Fixture()
    {
        effects::EffectManager::instance()->clear_content();
        resources::ResourceManager::instance()->clear();
        SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface); SDL_Quit();
    }
    core::Color pixel(int x = 40, int y = 30)
    {
        core::Color color;
        require(SDL_ReadSurfacePixel(surface, x, y, &color.r, &color.g, &color.b, &color.a), "read pixel");
        return color;
    }
    void clear()
    {
        require(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) && SDL_RenderClear(renderer), "clear");
    }
};

std::vector<core::UiRenderCommand> commands(const ScreenEffectRuntime& runtime)
{
    std::vector<core::UiRenderCommand> out;
    runtime.append_commands(ScreenEffectLayer::AfterUi, {0, 0, 80, 60}, out);
    return out;
}

void playback_tests()
{
    ScreenEffectRuntime runtime;
    int scene = 0, other = 0;
    ScreenColorEffectRequest r;
    r.playback.fade_in_seconds = 1;
    r.playback.hold_seconds = 1;
    r.playback.fade_out_seconds = 1;
    auto h = runtime.create(r, &scene);
    require(h.has_value(), "valid color request");
    require(commands(runtime).front().color.a == 0, "starts transparent");
    runtime.update(0.5, 0.1, false);
    require(commands(runtime).front().color.a == 128, "linear fade in uses raw delta");
    require(runtime.stop(*h), "stop during fade in");
    require(!runtime.stop(*h), "repeated stop does not restart fade");
    runtime.update(0.5, 0, true);
    require(commands(runtime).front().color.a == 64, "stop fades from current opacity while paused");
    runtime.update(0.5, 0, true);
    require(!runtime.active(*h) && !runtime.cancel(*h), "completion invalidates handle");
    auto next = runtime.create(r, &scene);
    require(next && next->slot == h->slot && next->generation != h->generation, "slot reuse changes generation");
    require(!runtime.stop(*h), "old handle cannot stop reused slot");
    runtime.update(2.5, 0, false);
    require(commands(runtime).front().color.a == 128, "large delta crosses fade in and hold");
    require(!runtime.stop(*next), "natural fade out cannot be restarted");
    runtime.update(20, 0, false);
    require(!runtime.active(*next), "large delta retires effect");

    r.playback.end = ScreenEffectEnd::Manual;
    r.playback.fade_in_seconds = 0;
    r.playback.clock = ScreenEffectClock::Scene;
    h = runtime.create(r, &scene);
    runtime.update(100, 100, true);
    require(runtime.active(*h) && commands(runtime).front().color.a == 255, "manual effect holds indefinitely");
    require(runtime.stop(*h), "manual stop");
    runtime.update(100, 1, true);
    require(commands(runtime).front().color.a == 255, "scene clock pauses");
    runtime.update(100, 0.25, false);
    require(commands(runtime).front().color.a == 191, "scene clock uses scaled time");
    auto unrelated = runtime.create(r, &other);
    runtime.unbind(&scene);
    require(!runtime.active(*h) && runtime.active(*unrelated), "unbind clears only owning scene");
    runtime.clear();
    require(!runtime.active(*unrelated), "clear invalidates all handles");

    r.playback = {};
    r.playback.hold_seconds = 0;
    require(!runtime.create(r, &scene), "zero total duration rejected");
    r.playback.fade_out_seconds = 1;
    r.color = {255, 255, 255, 128};
    r.playback.target_opacity = 0.5;
    h = runtime.create(r, &scene);
    require(commands(runtime).front().color.a == 64, "fade out only starts opaque and multiplies color alpha");
    runtime.update(0.5, 0, false);
    require(commands(runtime).front().color.a == 32, "fade out only decreases opacity");
    runtime.clear();
    r.playback.fade_in_seconds = -1;
    require(!runtime.create(r, &scene), "negative duration rejected");
    r.playback.fade_in_seconds = std::numeric_limits<double>::infinity();
    require(!runtime.create(r, &scene), "infinite duration rejected");
    r.playback = {};
    r.playback.target_opacity = std::numeric_limits<double>::quiet_NaN();
    require(!runtime.create(r, &scene), "NaN opacity rejected");
    r.playback.target_opacity = 1.1;
    require(!runtime.create(r, &scene), "out of range opacity rejected");
    r.playback = {};
    require(!runtime.create(r, nullptr), "missing scene rejected");
    r.playback.end = ScreenEffectEnd::Manual;
    r.playback.hold_seconds = 0;
    h = runtime.create(r, &scene);
    require(h && commands(runtime).front().color.a == 128, "zero duration manual request starts immediately");
    require(runtime.stop(*h) && !runtime.active(*h), "zero fade out stop immediately retires effect");
    r.playback = {};

    auto first = runtime.create(r, &scene);
    r.color = {255, 0, 0};
    (void)runtime.create(r, &scene);
    runtime.cancel(*first);
    r.color = {0, 255, 0};
    (void)runtime.create(r, &scene);
    auto out = commands(runtime);
    require(out.size() == 2 && out[0].color.r == 255 && out[1].color.g == 255, "creation order survives slot reuse");
}

void image_tests(Fixture& fixture)
{
    ScreenEffectRuntime color_runtime;
    int color_scene = 0;
    ScreenColorEffectRequest color;
    color.color = {255, 255, 255, 128};
    color.playback.target_opacity = 0.5;
    require(color_runtime.create(color, &color_scene).has_value(), "create blended color");
    fixture.clear();
    core::require_render_success(core::execute_render_commands(fixture.renderer, commands(color_runtime)));
    require(SDL_RenderPresent(fixture.renderer), "present color blend");
    require(std::abs(int(fixture.pixel().r) - 64) <= 2, "color alpha and playback opacity multiply in renderer");
    auto* pixels = SDL_CreateSurface(40, 10, SDL_PIXELFORMAT_RGBA32);
    require(pixels != nullptr, "image surface");
    require(SDL_FillSurfaceRect(pixels, nullptr, SDL_MapSurfaceRGBA(pixels, 255, 0, 0, 128)), "image fill");
    auto* texture = SDL_CreateTextureFromSurface(fixture.renderer, pixels);
    SDL_DestroySurface(pixels);
    require(texture && SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE), "image texture");
    ScreenEffectRuntime runtime;
    int scene = 0;
    ScreenImageEffectRequest r;
    r.playback.target_opacity = 0.5;
    auto h = runtime.create(r, texture, &scene);
    require(h.has_value(), "image request");
    auto out = commands(runtime);
    require(out.front().screen_rect == core::Rect(0, 0, 80, 60), "stretch fills viewport");
    fixture.clear();
    core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {0, 0, 80, 60}));
    require(SDL_RenderPresent(fixture.renderer), "present stretch");
    require(std::abs(int(fixture.pixel().r) - 64) <= 2, "image alpha multiplied by playback alpha");
    SDL_BlendMode blend;
    require(SDL_GetTextureBlendMode(texture, &blend) && blend == SDL_BLENDMODE_NONE, "borrowed texture blend mode restored");
    core::detail::render_operation_probe = [](std::string_view operation) {
        return operation != "SDL_RenderTextureRotated";
    };
    auto failure = runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {0, 0, 80, 60});
    core::detail::render_operation_probe = nullptr;
    require(!failure && failure.error().operation == "SDL_RenderTextureRotated", "SDL failure retains render boundary diagnostic");
    require(SDL_GetTextureBlendMode(texture, &blend) && blend == SDL_BLENDMODE_NONE, "failed render restores texture blend mode");
    Uint8 texture_alpha = 0;
    require(SDL_GetTextureAlphaMod(texture, &texture_alpha) && texture_alpha == 255, "failed render restores texture alpha modulation");
    runtime.cancel(*h);
    r.fit = ScreenEffectFit::Contain;
    h = runtime.create(r, texture, &scene);
    out = commands(runtime);
    require(out.front().screen_rect == core::Rect(0, 20, 80, 20), "contain centers without distortion");
    fixture.clear();
    core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {0, 0, 80, 60}));
    require(SDL_RenderPresent(fixture.renderer), "present contain");
    require(fixture.pixel(40, 0).r == 0 && fixture.pixel().r > 60, "contain leaves transparent margins");
    runtime.cancel(*h);
    r.fit = ScreenEffectFit::Cover;
    h = runtime.create(r, texture, &scene);
    out = commands(runtime);
    require(out.front().screen_rect == core::Rect(-80, 0, 240, 60), "cover centers oversized image");
    require(out.front().use_clip_rect && out.front().clip_rect == core::Rect(0, 0, 80, 60), "cover clips to viewport");
    std::vector<core::UiRenderCommand> inset;
    runtime.append_commands(ScreenEffectLayer::AfterUi, {10, 10, 60, 40}, inset);
    fixture.clear();
    core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {10, 10, 60, 40}));
    require(SDL_RenderPresent(fixture.renderer), "present cover");
    require(fixture.pixel(5, 30).r == 0 && fixture.pixel().r > 60, "cover does not draw outside viewport");
    require(!runtime.create(r, nullptr, &scene), "missing texture rejected");
    runtime.clear();
    auto* rgb = SDL_CreateSurface(40, 10, SDL_PIXELFORMAT_RGB24);
    require(rgb && SDL_FillSurfaceRect(rgb, nullptr, SDL_MapSurfaceRGB(rgb, 255, 255, 255)), "opaque image surface");
    auto* opaque = SDL_CreateTextureFromSurface(fixture.renderer, rgb);
    SDL_DestroySurface(rgb);
    require(opaque != nullptr && runtime.create(r, opaque, &scene).has_value(), "opaque image request");
    fixture.clear();
    core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {0, 0, 80, 60}));
    require(SDL_RenderPresent(fixture.renderer), "present opaque image fade");
    require(std::abs(int(fixture.pixel().r) - 128) <= 2, "opaque RGB image honors playback opacity");
    runtime.clear();
    SDL_DestroyTexture(opaque);
    require(resources::ResourceManager::instance()->store_texture("screen.test", resources::TexturePtr(texture)).has_value(), "register image resource");
}

class TestUi final : public ui::UiElement
{
public:
    void submit_ui_render_commands(std::vector<core::UiRenderCommand>& out) const override
    { out.push_back(core::make_ui_fill_rect_command({0, 0, 40, 60}, {0, 255, 0})); }
};
class TestScene final : public scene::Scene
{
public:
    TestScene() : Scene(scene::SceneRuntimeFeatures{.camera = scene::CameraSceneConfig{}}) {}
    void move_test_camera()
    {
        camera_runtime().set_center(camera::CameraSlot::Main, {1000, 1000});
        camera_runtime().set_zoom(camera::CameraSlot::Main, 3);
    }
    static inline TestScene* current = nullptr;
    static inline bool fail = false;
    static inline bool spawn = false;
    static inline bool spawn_on_update = false;
    static inline std::optional<ScreenEffectHandle> entering;
    static inline std::optional<ScreenEffectHandle> updating;
    void on_enter(const scene::ScenePayload&) override
    {
        current = this;
        create_and_add_object<TestUi>();
        if (spawn)
        {
            ScreenColorEffectRequest r;
            r.color = {0, 0, 0}; r.playback.hold_seconds = 0; r.playback.fade_out_seconds = 1;
            entering = ELYSIA_EFFECTS->request_screen_color_effect(r);
        }
    }
    void on_exit() override {}
    void on_reset() override {}
    void on_before_update(double) override
    {
        if (fail) throw std::runtime_error("intentional scene failure");
        if (spawn_on_update)
        {
            spawn_on_update = false;
            ScreenColorEffectRequest r;
            r.playback.fade_in_seconds = 1;
            updating = ELYSIA_EFFECTS->request_screen_color_effect(r);
        }
    }
};

void lifecycle_tests(Fixture& fixture)
{
    auto* service = ELYSIA_EFFECTS;
    ScreenColorEffectRequest r;
    require(!service->request_screen_color_effect(r), "service requires active scene");
    scene::SceneManager manager;
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context(fixture.renderer, registry, 80, 60);
    manager.initialize(context);
    manager.register_game_scene<TestScene>(101);
    manager.register_game_scene<TestScene>(102);
    manager.start({.target = 101});
    r.playback.end = ScreenEffectEnd::Manual;
    r.color = {255, 0, 0}; r.playback.layer = ScreenEffectLayer::BeforeUi;
    auto h = service->request_screen_color_effect(r);
    require(h.has_value(), "service creates color effect");
    TestScene::current->move_test_camera();
    core::Time::instance()->begin_frame(0);
    manager.on_update(0);
    fixture.clear(); manager.on_render(fixture.renderer);
    require(SDL_RenderPresent(fixture.renderer), "present before UI");
    require(fixture.pixel(20, 30).g == 255 && fixture.pixel(60, 30).r == 255, "before UI and camera independence");
    service->cancel_screen_effect(*h);
    r.playback.layer = ScreenEffectLayer::AfterUi;
    h = service->request_screen_color_effect(r);
    fixture.clear(); manager.on_render(fixture.renderer);
    require(SDL_RenderPresent(fixture.renderer), "present after UI");
    require(fixture.pixel(20, 30).r == 255 && fixture.pixel(60, 30).r == 255, "after UI covers UI and world");

    TestScene::spawn = true;
    manager.on_scene_request({.type = scene::SceneRequestType::Switch, .route = {.target = 102}});
    core::Time::instance()->begin_frame(0.5);
    manager.on_input({});
    manager.on_update(0.5);
    require(!service->is_screen_effect_active(*h), "scene switch invalidates old handle");
    require(TestScene::entering && service->is_screen_effect_active(*TestScene::entering), "new scene creates own fade out");
    fixture.clear(); manager.on_render(fixture.renderer);
    require(SDL_RenderPresent(fixture.renderer), "present entering black");
    require(fixture.pixel(20, 30).g == 0, "new scene first frame is black");
    core::Time::instance()->begin_frame(0.5);
    manager.on_update(0.5);
    fixture.clear(); manager.on_render(fixture.renderer);
    require(SDL_RenderPresent(fixture.renderer), "present fading black");
    require(std::abs(int(fixture.pixel(20, 30).g) - 127) <= 2, "new scene fades independently");
    auto previous = *TestScene::entering;
    manager.on_scene_request({.type = scene::SceneRequestType::Switch, .route = {.target = 101}});
    core::Time::instance()->begin_frame(0);
    manager.on_update(0);
    require(!service->is_screen_effect_active(previous), "cached scene switch also clears effects");
    service->cancel_screen_effect(*TestScene::entering);
    r.playback = {}; r.playback.fade_in_seconds = 1; r.playback.hold_seconds = 1;
    r.playback.clock = ScreenEffectClock::Scene;
    h = service->request_screen_color_effect(r);
    TestScene::current->pause();
    core::Time::instance()->set_time_scale(0.5);
    core::Time::instance()->begin_frame(0.5); manager.on_update(0.25);
    std::vector<core::UiRenderCommand> out;
    EffectManager::instance()->append_screen_effect_commands(ScreenEffectLayer::AfterUi, {0, 0, 80, 60}, out);
    require(out.front().color.a == 0, "scene clock pauses through manager");
    TestScene::current->resume();
    core::Time::instance()->begin_frame(0.5); manager.on_update(0.25);
    out.clear(); EffectManager::instance()->append_screen_effect_commands(ScreenEffectLayer::AfterUi, {0, 0, 80, 60}, out);
    require(out.front().color.a == 64, "scene clock honors global scaling");
    service->cancel_screen_effect(*h);
    TestScene::spawn_on_update = true;
    core::Time::instance()->begin_frame(0.5); manager.on_update(0.25);
    out.clear(); EffectManager::instance()->append_screen_effect_commands(ScreenEffectLayer::AfterUi, {0, 0, 80, 60}, out);
    require(out.front().color.a == 0, "effect spawned in scene update does not advance that frame");
    TestScene::current->pause();
    core::Time::instance()->begin_frame(0.5); manager.on_update(0.25);
    out.clear(); EffectManager::instance()->append_screen_effect_commands(ScreenEffectLayer::AfterUi, {0, 0, 80, 60}, out);
    require(out.front().color.a == 128, "default clock continues despite pause and global scaling");
    TestScene::current->resume();
    h = TestScene::updating;
    ScreenImageEffectRequest image;
    image.texture_key = "missing";
    require(!service->request_screen_image_effect(image), "missing resource rejected");
    image.texture_key = "screen.test";
    auto ih = service->request_screen_image_effect(image);
    require(ih.has_value(), "service resolves texture key");
    loading::clear_loaded_content();
    require(!service->is_screen_effect_active(*ih) && !service->is_screen_effect_active(*h), "content cleanup invalidates handles before textures");
    h = service->request_screen_color_effect(r);
    TestScene::fail = true;
    manager.on_update(0);
    require(!service->is_screen_effect_active(*h), "scene failure clears screen effects");
    TestScene::fail = false;
    require(manager.shutdown(), "shutdown after fault");
    require(!service->request_screen_color_effect(r), "shutdown unbinds service");
    core::Time::instance()->reset();
    TestScene::spawn = false;
    scene::SceneManager shutdown_manager;
    shutdown_manager.initialize(context);
    shutdown_manager.register_game_scene<TestScene>(101);
    shutdown_manager.start({.target = 101});
    h = service->request_screen_color_effect(r);
    require(shutdown_manager.shutdown() && !service->is_screen_effect_active(*h), "normal shutdown invalidates handles");
}
}

int main()
{
    Fixture fixture;
    playback_tests(); image_tests(fixture); lifecycle_tests(fixture);
    return 0;
}
