#define SDL_MAIN_HANDLED
#include "particle_test_fixture.h"
#include "effect_request_assertions.h"
#include "engine/core/time.h"
#include "engine/core/render/render_command.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/loading/content_runtime_cleanup.h"
#include <limits>
#include <stdexcept>

using namespace elysia;
using namespace elysia::effects;
using tests::require;
using tests::require_effect_failure;
namespace
{
class TestScene final : public scene::Scene
{
public:
    TestScene() : Scene({.camera=scene::CameraSceneConfig{}}) {}
    static inline TestScene* current=nullptr;
    static inline bool fail=false;
    static inline bool spawn=false;
    static inline ParticleEffectHandle entering;
private:
    void on_enter(const scene::ScenePayload&) override
    {
        current=this;
        if(spawn) {auto result=ELYSIA_EFFECTS->request_particle_effect({.effect_key="particle.test"});require(result,"entry particle request");entering=*result;}
    }
    void on_before_update(double) override {if(fail) throw std::runtime_error("intentional particle scene fault");}
    void on_exit() override {}
    void on_reset() override {}
};
class LayerBackdrop final : public core::GameObject
{
public:
    LayerBackdrop() : GameObject(core::DepthLayer::EffectFront) {}
    void submit_render_commands(std::vector<core::RenderCommand>& out) const override
    { out.push_back(core::make_world_fill_rect_command(core::Rect{-20,-20,40,40}, core::Color{0,0,255})); }
};
class NoCamera final : public scene::Scene
{
    void on_enter(const scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};
class FailedEntry final : public scene::Scene
{
public:
    FailedEntry() : Scene({.camera=scene::CameraSceneConfig{}}) {}
    static inline ParticleEffectHandle handle;
private:
    void on_enter(const scene::ScenePayload&) override
    {
        auto result=ELYSIA_EFFECTS->request_particle_effect({.effect_key="particle.test"});require(result,"failed entry first creates group");handle=*result;
        throw std::runtime_error("intentional particle entry failure");
    }
    void on_exit() override {}
    void on_reset() override {}
};
void lifecycle(tests::ParticleFixture& f)
{
    auto* service=ELYSIA_EFFECTS;auto* effects=EffectManager::instance();
    require_effect_failure(service->request_particle_effect({.effect_key="missing"}),EffectRequestError::NoActiveScene);
    auto d=f.definition();d.lifetime_seconds={1,1};
    require(effects->register_particle_effect(d),"register particle fixture definition in the manager");
    effects->set_runtime_dependencies(f.renderer,nullptr);
    scene::SceneManager manager;io::ContentRegistry registry;scene::SceneRuntimeContext context(f.renderer,registry,128,96);
    manager.initialize(context);manager.register_game_scene<TestScene>(101);manager.register_game_scene<TestScene>(102);manager.register_game_scene<FailedEntry>(103);manager.register_game_scene<NoCamera>(104);
    manager.start({.target=101});
    const auto switch_scene = [&](scene::SceneKey key) {
        scene::SceneRequest request; request.type=scene::SceneRequestType::Switch;
        request.route.target=key; request.route.reload_mode=scene::SceneReloadMode::Reuse;
        manager.on_scene_request(request); core::Time::instance()->begin_frame(0); manager.on_update(0);
        return manager.current_scene_key()==key;
    };
    require_effect_failure(service->request_particle_effect({.effect_key="missing"}),EffectRequestError::MissingEffectDefinition);
    require_effect_failure(service->request_particle_effect({.effect_key=d.effect_key,.position={std::numeric_limits<float>::infinity(),0}}),EffectRequestError::InvalidRequest);
    auto first=service->request_particle_effect({.effect_key=d.effect_key});require(first && effects->particle_count()==1,"burst created immediately");
    manager.on_update(1);require(service->is_particle_effect_active(*first),"creation frame is not advanced");
    // Re-registering definitions changes only future instances.
    d.burst_count=2;require(effects->register_particle_effect(d),"replace definition");
    auto second=service->request_particle_effect({.effect_key=d.effect_key});require(second && effects->particle_count()==3,"old instance retains its snapshot");
    require(service->cancel_particle_effect(*second),"cancel second instance");
    f.clear();manager.on_render(f.renderer);auto* image=SDL_RenderReadPixels(f.renderer,nullptr);require(image,"empty-layer render capture");
    core::Color center;require(SDL_ReadSurfacePixel(image,64,48,&center.r,&center.g,&center.b,&center.a),"read empty layer");SDL_DestroySurface(image);
    require(center.r>240,"particle draws even when its world layer has no ordinary objects");
    require(TestScene::current->create_and_add_object<LayerBackdrop>(), "ordinary object in particle depth layer");
    f.clear(); manager.on_render(f.renderer); image=SDL_RenderReadPixels(f.renderer,nullptr); require(image, "layer-order capture");
    require(SDL_ReadSurfacePixel(image,64,48,&center.r,&center.g,&center.b,&center.a), "layer-order sample"); SDL_DestroySurface(image);
    require(center.r>240 && center.b<10, "particle groups render after ordinary objects in the same depth layer");
    TestScene::current->pause();core::Time::instance()->begin_frame(2);manager.on_update(2);
    require(service->is_particle_effect_active(*first),"scene pause freezes particles");TestScene::current->resume();
    core::Time::instance()->set_time_scale(0.5);core::Time::instance()->begin_frame(1);manager.on_update(core::Time::instance()->delta());
    require(service->is_particle_effect_active(*first),"scaled delta advances half a lifetime");
    core::Time::instance()->begin_frame(1);manager.on_update(core::Time::instance()->delta());require(!service->is_particle_effect_active(*first),"scaled time eventually completes the group");core::Time::instance()->set_time_scale(1);
    auto cached=service->request_particle_effect({.effect_key=d.effect_key});require(cached,"cached-scene particle");
    require(switch_scene(102),"switch scene");require(!service->is_particle_effect_active(*cached),"reuse exit invalidates groups");
    require(switch_scene(101),"return cached scene");require(effects->particle_count()==0,"cached scene does not revive old groups");
    require(switch_scene(104), "enter a UI-only scene");
    require_effect_failure(service->request_particle_effect({.effect_key=d.effect_key}),EffectRequestError::RuntimeUnavailable);
    f.clear(); manager.on_render(f.renderer);
    require(switch_scene(101), "return from UI-only scene");
    auto before_reset=service->request_particle_effect({.effect_key=d.effect_key});require(before_reset,"before dependency detach");effects->set_runtime_dependencies(nullptr,nullptr);
    require(!service->is_particle_effect_active(*before_reset),"renderer detach clears borrowed textures");
    require_effect_failure(service->request_particle_effect({.effect_key=d.effect_key}),EffectRequestError::RuntimeUnavailable);effects->set_runtime_dependencies(f.renderer,nullptr);
    auto fault=service->request_particle_effect({.effect_key=d.effect_key});require(fault,"before scene fault");TestScene::fail=true;
    core::Time::instance()->begin_frame(0);manager.on_update(0);TestScene::fail=false;
    require(!service->is_particle_effect_active(*fault),"scene failure recovery invalidates groups");require(manager.shutdown(),"shutdown after failure");
    scene::SceneManager failed_entry; failed_entry.initialize(context); failed_entry.register_game_scene<FailedEntry>(103);
    failed_entry.start({.target=103});
    require(FailedEntry::handle.generation!=0 && !service->is_particle_effect_active(FailedEntry::handle), "failed entry cleans created particle groups");
    require(failed_entry.shutdown(), "shutdown failed entry manager");
    core::Time::instance()->reset();TestScene::spawn=true;
    scene::SceneManager next;next.initialize(context);next.register_game_scene<TestScene>(101);next.start({.target=101});TestScene::spawn=false;
    require(service->is_particle_effect_active(TestScene::entering),"particles can spawn during entry");
    effects->clear_transient_effects();resources::ResourceManager::instance()->clear();
    require_effect_failure(service->request_particle_effect({.effect_key=d.effect_key}),EffectRequestError::MissingTexture);
    require(!service->is_particle_effect_active(TestScene::entering),"clearing transient effects invalidates handles");
    require(next.shutdown(),"normal shutdown");
    require_effect_failure(service->request_particle_effect({.effect_key=d.effect_key}),EffectRequestError::NoActiveScene);
    resources::TexturePtr replacement(SDL_CreateTexture(f.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 8, 4));
    require(replacement && resources::ResourceManager::instance()->store_texture("particle.test", std::move(replacement)), "reload texture for cleanup verification");
    require(effects->register_particle_effect(d), "reload code definition");
    scene::SceneManager cleanup; cleanup.initialize(context); cleanup.register_game_scene<TestScene>(101); cleanup.start({.target=101});
    auto live=service->request_particle_effect({.effect_key=d.effect_key}); require(live,"live group before content cleanup");
    loading::clear_loaded_content();
    require(!service->is_particle_effect_active(*live) && effects->particle_count()==0, "content cleanup invalidates live groups before releasing textures");
    require(cleanup.shutdown(), "shutdown after content cleanup");
}
}
int main(){tests::ParticleFixture fixture;lifecycle(fixture);}
