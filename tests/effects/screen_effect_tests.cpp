#define SDL_MAIN_HANDLED
#include "engine/effects/screen/screen_effect_runtime.h"
#include "engine/effects/effect_service.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/core/time.h"
#include "engine/scene/scene_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/texture/texture_loader.h"
#include "engine/ui/core/ui_element.h"
#include "engine/loading/content_runtime_cleanup.h"
#include "engine/animation/animation_service.h"
#include "engine/animation/runtime/animation_manager.h"
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
    r.fit = ScreenEffectFit::Natural;
    r.placement.anchor = EffectAnchor::BottomRight;
    r.placement.offset = {-2, -3};
    r.placement.scale = {0.5f, 2};
    require(runtime.create(r, texture, &scene).has_value(), "image accepts shared placement");
    require(commands(runtime).front().screen_rect == core::Rect(58, 37, 20, 20), "natural image placement and scale");
    runtime.clear();
    r.placement.scale.x = 0;
    require(!runtime.create(r, texture, &scene), "zero image scale rejected");
    r.placement = {};
    r.playback.end = ScreenEffectEnd::AnimationFinished;
    require(!runtime.create(r, texture, &scene), "image rejects animation completion policy");
    require(resources::ResourceManager::instance()->store_texture("screen.test", resources::TexturePtr(texture)).has_value(), "register image resource");
}

void animation_tests(Fixture& fixture)
{
    auto* surface = SDL_CreateSurface(8, 4, SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr, "animation sheet surface");
    SDL_Rect left{0, 0, 4, 4}, right{4, 0, 4, 4};
    require(SDL_FillSurfaceRect(surface, &left, SDL_MapSurfaceRGBA(surface, 255, 0, 0, 128))
        && SDL_FillSurfaceRect(surface, &right, SDL_MapSurfaceRGBA(surface, 0, 255, 0, 128)), "animation sheet colors");
    resources::TexturePtr texture(SDL_CreateTextureFromSurface(fixture.renderer, surface));
    SDL_DestroySurface(surface);
    require(texture != nullptr, "animation sheet texture");
    resources::Atlas atlas("screen.animation.atlas");
    require(atlas.add_frame({}, texture.get(), texture.get(), core::Rect{0, 0, 4, 4})
        && atlas.add_frame({}, texture.get(), texture.get(), core::Rect{4, 0, 4, 4}), "atlas source rectangles");
    animation::AnimationDefinition definition{.fps = 2, .loop = true, .atlas = &atlas};
    ScreenEffectRuntime runtime;
    int scene = 0;
    auto make = [&] {
        auto animation = std::make_unique<animation::Animation>();
        animation->set_atlas(&atlas); animation->set_interval_seconds(0.5);
        return animation;
    };
    ScreenAnimationEffectRequest r;
    const auto create = [&]() { return runtime.create(r, make(), definition, {8, 6}, 0, &scene); };
    auto h = create();
    require(h && commands(runtime).front().src_rect.x() == 0, "default single shot starts on first frame");
    runtime.update(0.5, 0, false);
    require(commands(runtime).front().src_rect.x() == 4 && runtime.active(*h), "last frame receives its full interval");
    runtime.update(0.5, 0, false);
    require(!runtime.active(*h), "default ends after all full frame intervals");
    r.playback.fade_in_seconds = 2; r.playback.fade_out_seconds = 1;
    h = create(); runtime.update(1, 0, false);
    require(commands(runtime).front().src_rect.x() == 4 && commands(runtime).front().alpha == 128,
        "animation and fade in advance together; completed animation holds last frame");
    runtime.update(1.5, 0, false);
    require(commands(runtime).front().alpha == 128, "large delta carries remainder into completion fade out");
    require(!runtime.stop(*h), "automatic completion fade cannot restart");
    runtime.update(0.5, 0, false); require(!runtime.active(*h), "completion fade retires");
    r.playback.fade_in_seconds = 0; h = create(); runtime.update(50, 0, false);
    require(!runtime.active(*h), "large delta completes animation and fade");

    r.playback.end = ScreenEffectEnd::Manual; r.playback.fade_out_seconds = 1;
    r.loop = true; r.playback.clock = ScreenEffectClock::Scene;
    h = create(); runtime.update(10, 0.5, true);
    require(commands(runtime).front().src_rect.x() == 0, "scene pause stops animation clock");
    runtime.update(10, 0.5, false);
    require(commands(runtime).front().src_rect.x() == 4, "scene clock uses scaled delta");
    require(runtime.stop(*h) && !runtime.stop(*h), "manual animation stop is idempotent");
    runtime.update(0, 0.5, false);
    require(commands(runtime).front().src_rect.x() == 0 && commands(runtime).front().alpha == 128,
        "loop continues during manual fade out");
    runtime.cancel(*h);
    auto reused = create();
    require(reused->slot == h->slot && reused->generation != h->generation && !runtime.cancel(*h), "animation handle reuse safe");
    runtime.clear();
    r.loop = false; r.playback.clock = ScreenEffectClock::Unscaled;
    r.playback.end = ScreenEffectEnd::Timed; r.playback.hold_seconds = 3;
    h = create(); runtime.update(2, 0, false);
    require(commands(runtime).front().src_rect.x() == 4 && commands(runtime).front().alpha == 255,
        "timed single shot holds final frame until hold ends");
    runtime.clear();

    r.playback.end = ScreenEffectEnd::Manual; r.playback.target_opacity = 0.5;
    r.fit = ScreenEffectFit::Natural; r.placement.anchor = EffectAnchor::BottomRight;
    r.placement.offset = {-2, -3}; r.placement.scale = {2, 2};
    r.flip = core::SpriteFlip::Horizontal;
    h = create(); auto out = commands(runtime);
    require(out.front().screen_rect == core::Rect(62, 45, 16, 12) && out.front().flip == r.flip,
        "natural animation uses effect size, placement and flip");
    runtime.clear(); r.fit = ScreenEffectFit::Contain; r.placement = {}; r.flip = core::SpriteFlip::None;
    h = create(); fixture.clear();
    core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {0, 0, 80, 60}));
    require(SDL_RenderPresent(fixture.renderer), "present animation");
    require(std::abs(int(fixture.pixel().r) - 64) <= 2 && fixture.pixel().g == 0, "atlas alpha and opacity multiply");
    runtime.update(0.5, 0, false); fixture.clear();
    core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {0, 0, 80, 60}));
    require(SDL_RenderPresent(fixture.renderer) && fixture.pixel().g > 60 && fixture.pixel().r == 0, "renderer uses selected atlas frame");
    runtime.clear();
    for (int anchor = 0; anchor < 9; ++anchor)
    {
        r.fit = ScreenEffectFit::Natural; r.placement.anchor = static_cast<EffectAnchor>(anchor);
        h = create(); out = commands(runtime);
        require(out.front().screen_rect == core::Rect((80-8)*(anchor%3)*0.5f, (60-6)*(anchor/3)*0.5f, 8, 6), "nine anchors align corresponding points");
        runtime.clear();
    }
    r.placement = {}; r.fit = ScreenEffectFit::Stretch; h = create();
    require(commands(runtime).front().screen_rect == core::Rect(0, 0, 80, 60), "animation stretch"); runtime.clear();
    r.fit = ScreenEffectFit::Cover; h = create();
    require(commands(runtime).front().screen_rect == core::Rect(0, -10, 80, 80), "animation cover uses first frame aspect"); runtime.clear();
    r.fit = ScreenEffectFit::Contain; h = create();
    require(commands(runtime).front().screen_rect == core::Rect(10, 0, 60, 60), "animation contain uses first frame aspect"); runtime.clear();
    r.playback.end = ScreenEffectEnd::AnimationFinished; r.loop = true;
    require(!create(), "loop and animation completion are incompatible");
    r.loop = false; r.placement.offset.x = std::numeric_limits<float>::quiet_NaN();
    require(!create(), "nonfinite placement rejected"); r.placement = {};
    require(!runtime.create(r, make(), definition, {8, 6}, std::numeric_limits<double>::infinity(), &scene), "nonfinite rotation rejected");
    require(!runtime.create(r, make(), definition, {8, 6}, 0, nullptr), "animation requires scene");
    definition.fps = 0; require(!create(), "invalid animation FPS rejected"); definition.fps = 2;
    r.playback = {.end = ScreenEffectEnd::AnimationFinished, .hold_seconds = 0};
    h = runtime.create(r, make(), definition, {}, 0, &scene, 7);
    runtime.update(1, 1, false, 7); require(runtime.active(*h), "creation frame never advances animation");
    runtime.update(0.5, 0, false, 8); require(commands(runtime).front().src_rect.x() == 4, "next frame advances animation");
    runtime.unbind(&scene); require(!runtime.active(*h), "scene unbind clears animation handle");
    resources::Atlas single("single"); require(single.add_texture(texture.get(), texture.get()), "single frame atlas");
    definition.atlas = &single;
    auto one = std::make_unique<animation::Animation>(); one->set_atlas(&single); one->set_interval_seconds(0.5);
    h = runtime.create(r, std::move(one), definition, {}, 0, &scene);
    runtime.update(0.25, 0, false); require(runtime.active(*h), "single frame lasts a full interval");
    runtime.update(0.25, 0, false); require(!runtime.active(*h), "single frame completion");
    const auto full_frame = [&] {
        auto a = std::make_unique<animation::Animation>(); a->set_atlas(&single); a->set_interval_seconds(0.5); return a;
    };
    r.playback.end = ScreenEffectEnd::Manual; r.playback.target_opacity = 1; r.fit = ScreenEffectFit::Natural;
    r.flip = core::SpriteFlip::Horizontal;
    h = runtime.create(r, full_frame(), definition, {}, 0, &scene);
    fixture.clear(); core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {0,0,80,60}));
    require(SDL_RenderPresent(fixture.renderer) && fixture.pixel(38,30).g > 120 && fixture.pixel(42,30).r > 120,
        "software renderer applies animation horizontal flip"); runtime.clear();
    r.flip = core::SpriteFlip::None;
    h = runtime.create(r, full_frame(), definition, {}, 90, &scene);
    fixture.clear(); core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {0,0,80,60}));
    require(SDL_RenderPresent(fixture.renderer) && fixture.pixel(40,27).r > 120 && fixture.pixel(40,32).g > 120,
        "software renderer rotates around rectangle center"); runtime.clear();
    r.fit = ScreenEffectFit::Cover; r.placement.anchor = EffectAnchor::TopLeft;
    h = runtime.create(r, full_frame(), definition, {}, 0, &scene);
    fixture.clear(); core::require_render_success(runtime.render(fixture.renderer, ScreenEffectLayer::AfterUi, {10,10,60,40}));
    require(SDL_RenderPresent(fixture.renderer) && fixture.pixel(5,30).r == 0 && fixture.pixel(15,30).r > 120,
        "anchored animation cover clips outside logical viewport"); runtime.clear();
    resources::Atlas mixed("mixed");
    require(mixed.add_frame({},texture.get(),texture.get(),core::Rect{0,0,4,4})
        && mixed.add_frame({},texture.get(),texture.get(),core::Rect{4,0,4,2}), "variable frame dimensions");
    definition.atlas=&mixed;
    auto varied=std::make_unique<animation::Animation>();varied->set_atlas(&mixed);varied->set_interval_seconds(0.5);
    r.placement={};r.fit=ScreenEffectFit::Contain;
    h=runtime.create(r,std::move(varied),definition,{},0,&scene);
    const auto first_rect=commands(runtime).front().screen_rect;
    runtime.update(0.5,0,false);
    require(commands(runtime).front().screen_rect==first_rect && commands(runtime).front().src_rect.height()==2,
        "frame dimensions change source cropping without layout jitter");
    runtime.clear();
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
    resources::Atlas atlas("screen.lifecycle");
    auto* texture = ELYSIA_RESOURCES->find_texture("screen.test");
    require(atlas.add_frame({}, texture, texture, core::Rect{0, 0, 20, 10})
        && atlas.add_frame({}, texture, texture, core::Rect{20, 0, 20, 10}), "lifecycle animation atlas");
    resources::AnimationBuildRequest ar;
    ar.animation_key = "screen.lifecycle"; ar.atlas_key = "screen.lifecycle"; ar.fps = 2; ar.loop = true;
    require(animation::AnimationManager::instance()->register_animation(ar, &atlas), "lifecycle animation registration");
    resources::AnimationEffectBuildRequest er;
    er.effect_key = "screen.shared"; er.animation_key = ar.animation_key; er.default_angle_degrees = 15;
    require(EffectManager::instance()->register_animation_effect(er), "shared effect registration");
    ScreenAnimationEffectRequest sr; sr.effect_key = er.effect_key;
    sr.loop = true; sr.playback.end = ScreenEffectEnd::Manual;
    require(!service->request_screen_animation_effect(ScreenAnimationEffectRequest{.effect_key = "missing"}), "missing effect key rejected");
    AnimationEffect* world = nullptr;
    AnimationEffectSpawnRequest wr; wr.effect_key = er.effect_key;
    wr.on_started = [&](AnimationEffect& effect) { world = &effect; };
    require(service->request_animation_effect(wr), "same key dispatches world animation");
    auto first_animation = service->request_screen_animation_effect(sr);
    core::Time::instance()->begin_frame(0.5); manager.on_update(0.5);
    auto second_animation = service->request_screen_animation_effect(sr);
    std::vector<core::UiRenderCommand> shared;
    EffectManager::instance()->append_screen_effect_commands(ScreenEffectLayer::AfterUi, {0, 0, 80, 60}, shared);
    std::vector<core::RenderCommand> world_commands; world->submit_render_commands(world_commands);
    require(shared.size() == 2 && shared[0].src_rect.x() == 20 && shared[1].src_rect.x() == 0,
        "same effect key creates independent screen playback instances");
    require(world_commands.size() == 1 && world_commands[0].texture == shared[0].texture
        && shared[0].rotation_degrees == 15, "world and screen share texture and definition angle");
    service->cancel_screen_effect(*first_animation); service->cancel_screen_effect(*second_animation);
    world_commands.clear(); world->submit_render_commands(world_commands);
    require(world_commands.size() == 1, "cancelling screen playback leaves world instance intact");
    world->destroy();
    sr.playback.layer = ScreenEffectLayer::BeforeUi; sr.angle_degrees = 0;
    first_animation = service->request_screen_animation_effect(sr);
    TestScene::current->move_test_camera(); fixture.clear(); manager.on_render(fixture.renderer);
    require(SDL_RenderPresent(fixture.renderer) && fixture.pixel(20,30).g == 255 && fixture.pixel(60,30).r > 120,
        "screen animation before UI ignores camera movement and zoom");
    service->cancel_screen_effect(*first_animation);
    sr.playback.layer = ScreenEffectLayer::AfterUi;
    first_animation = service->request_screen_animation_effect(sr);
    fixture.clear(); manager.on_render(fixture.renderer);
    require(SDL_RenderPresent(fixture.renderer) && fixture.pixel(20,30).r > 120,
        "screen animation after UI blends over UI");
    service->cancel_screen_effect(*first_animation);
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

    first_animation = service->request_screen_animation_effect(sr);
    TestScene::spawn = true;
    manager.on_scene_request({.type = scene::SceneRequestType::Switch, .route = {.target = 102}});
    core::Time::instance()->begin_frame(0.5);
    manager.on_input({});
    manager.on_update(0.5);
    require(!service->is_screen_effect_active(*h), "scene switch invalidates old handle");
    require(!service->is_screen_effect_active(*first_animation), "normal switch clears screen animation");
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
    first_animation = service->request_screen_animation_effect(sr);
    manager.on_scene_request({.type = scene::SceneRequestType::Switch, .route = {.target = 101}});
    core::Time::instance()->begin_frame(0);
    manager.on_update(0);
    require(!service->is_screen_effect_active(previous), "cached scene switch also clears effects");
    require(!service->is_screen_effect_active(*first_animation), "cached reuse also clears screen animation");
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
    first_animation = service->request_screen_animation_effect(sr);
    loading::clear_loaded_content();
    require(!service->is_screen_effect_active(*ih) && !service->is_screen_effect_active(*h), "content cleanup invalidates handles before textures");
    require(!service->is_screen_effect_active(*first_animation), "content cleanup destroys borrowed animation before resources");
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

void animation_lifecycle_tests(Fixture& fixture)
{
    resources::TexturePtr texture(SDL_CreateTexture(fixture.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 8, 4));
    require(texture != nullptr, "lifecycle texture");
    resources::Atlas atlas("lifecycle.animation");
    require(atlas.add_frame({},texture.get(),texture.get(),core::Rect{0,0,4,4})
        && atlas.add_frame({},texture.get(),texture.get(),core::Rect{4,0,4,4}), "lifecycle atlas");
    resources::AnimationBuildRequest ar;
    ar.animation_key="lifecycle.animation"; ar.atlas_key="lifecycle.animation"; ar.fps=2;
    require(animation::AnimationManager::instance()->register_animation(ar,&atlas), "lifecycle animation");
    resources::AnimationEffectBuildRequest er;
    er.effect_key="lifecycle.effect"; er.animation_key=ar.animation_key;
    require(EffectManager::instance()->register_animation_effect(er), "lifecycle effect");
    TestScene::spawn=false;
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context(fixture.renderer,registry,80,60);
    scene::SceneManager manager; manager.initialize(context); manager.register_game_scene<TestScene>(101); manager.start({.target=101});
    ScreenAnimationEffectRequest r; r.effect_key=er.effect_key; r.loop=true;
    r.playback.end=ScreenEffectEnd::Manual; r.playback.clock=ScreenEffectClock::Scene;
    auto h=ELYSIA_EFFECTS->request_screen_animation_effect(r);
    require(h.has_value(), "scene-clock animation request");
    TestScene::current->pause(); core::Time::instance()->begin_frame(1); manager.on_update(1);
    std::vector<core::UiRenderCommand> out;
    const auto snapshot=[&] {
        out.clear();EffectManager::instance()->append_screen_effect_commands(ScreenEffectLayer::AfterUi,{0,0,80,60},out);
    };
    snapshot();require(out.front().src_rect.x()==0,"manager pauses scene animation clock");
    TestScene::current->resume();core::Time::instance()->set_time_scale(0.5);
    core::Time::instance()->begin_frame(1);manager.on_update(0.5);snapshot();
    require(out.front().src_rect.x()==4,"manager scales scene animation clock");
    r.playback.clock=ScreenEffectClock::Unscaled;
    auto raw=ELYSIA_EFFECTS->request_screen_animation_effect(r);
    TestScene::current->pause();core::Time::instance()->begin_frame(0.5);manager.on_update(0.25);snapshot();
    require(out.back().src_rect.x()==4,"unscaled animation advances during pause and scaling");
    TestScene::current->resume();TestScene::fail=true;manager.on_update(0);
    require(!ELYSIA_EFFECTS->is_screen_effect_active(*h) && !ELYSIA_EFFECTS->is_screen_effect_active(*raw),"fault recovery invalidates all screen animations");
    TestScene::fail=false;require(manager.shutdown(),"faulted animation manager shuts down");
    core::Time::instance()->reset();
    scene::SceneManager next;next.initialize(context);next.register_game_scene<TestScene>(101);next.start({.target=101});
    h=ELYSIA_EFFECTS->request_screen_animation_effect(r);
    require(h && next.shutdown() && !ELYSIA_EFFECTS->is_screen_effect_active(*h),"normal shutdown clears animation handles");
    require(!ELYSIA_EFFECTS->request_screen_animation_effect(r),"animation service rejects request after shutdown");
    EffectManager::instance()->clear_content();animation::AnimationManager::instance()->clear();
}
}

int main()
{
    Fixture fixture;
    playback_tests(); image_tests(fixture); animation_tests(fixture); lifecycle_tests(fixture); animation_lifecycle_tests(fixture);
    return 0;
}
