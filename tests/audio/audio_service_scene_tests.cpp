#define SDL_MAIN_HANDLED
#include "engine/audio/audio_service.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/scene/scene_manager.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"
#include <cmath>
#include <filesystem>
#include <stdexcept>

using namespace elysia;
using namespace elysia::audio;
using namespace std::chrono_literals;
using tests::require;
using Access = scene::SceneTestAccess;

namespace
{
const SoundDistanceConfig distance{10,40,50,2};
void near(double a,double b) { require(std::abs(a-b)<1e-6,"service preserves composed gain"); }
MIX_Track* track_with_gain(double gain)
{
    for(auto* track:audio::detail::mixer_backend().tracks)
        if((MIX_TrackPlaying(track)||MIX_TrackPaused(track))&&std::abs(MIX_GetTrackGain(track)-gain)<1e-6) return track;
    throw std::runtime_error("Expected fixture voice was not found");
}
struct State
{
    int detach_calls=0;
    bool attached=false,detached=false,entered=false,exited=false;
    bool fail_attach=false,fail_detach=false,fail_enter=false,fail_exit=false;
    SoundHandle sound=0,positioned=0,ui=0,delayed=0;
    std::optional<LoopSoundId> loop;
};
class TestScene final : public scene::Scene
{
public:
    TestScene(State& state) : s(state) {}
    static inline TestScene* current=nullptr;
private:
    State& s;
    void on_runtime_attach() override
    {
        s.attached=ELYSIA_AUDIO->scene_audio_available();
        if(s.fail_attach)
        {
            s.sound=ELYSIA_AUDIO->request_sound("loop",{.loops=-1})->handle;
            throw std::runtime_error("attach failure");
        }
    }
    void on_runtime_detach() override
    {
        ++s.detach_calls;
        s.detached=!ELYSIA_AUDIO->scene_audio_available();
        if(s.fail_detach) throw std::runtime_error("detach failure");
    }
    void on_enter(const scene::ScenePayload&) override
    {
        current=this;
        s.entered=ELYSIA_AUDIO->scene_audio_available();
        s.sound=ELYSIA_AUDIO->request_sound("loop",{.loops=-1,.group=SoundGroup::Gameplay,.gain=.8})->handle;
        s.positioned=ELYSIA_AUDIO->request_sound("loop",{.loops=-1,.gain=.8,.spatial=SpatialSoundConfig{{20,0},distance}})->handle;
        s.ui=ELYSIA_AUDIO->request_sound("loop",{.loops=-1,.group=SoundGroup::Ui,.gain=.6,.pause_policy=SoundPausePolicy::ContinueWhenPaused})->handle;
        s.delayed=ELYSIA_AUDIO->request_sound("loop",{.group=SoundGroup::Gameplay,.start_delay=1s})->handle;
        s.loop=ELYSIA_AUDIO->create_loop({.key="loop",.distance=distance,.gain=.7}).value();
        if(s.fail_enter) throw std::runtime_error("enter failure");
    }
    void on_exit() override
    {
        s.exited=!ELYSIA_AUDIO->scene_audio_available();
        if(s.fail_exit) throw std::runtime_error("exit failure");
    }
    void on_reset() override {}
};

void unavailable()
{
    require(!ELYSIA_AUDIO->scene_audio_available()&&!ELYSIA_AUDIO->listener_position(),"no scene capability outside attached scope");
    require(!ELYSIA_AUDIO->set_listener_position({0,0})&&!ELYSIA_AUDIO->clear_scene_sounds(),"settings reject missing runtime");
    require(!ELYSIA_AUDIO->request_sound("loop"),
        "scene requests do not fall back to global playback");
    require(!ELYSIA_AUDIO->create_loop({.key="loop",.distance=distance})&&!ELYSIA_AUDIO->loop_state({1}),"logical sources require capability");
    require(!ELYSIA_AUDIO->set_loop_gain({1},.5)&&!ELYSIA_AUDIO->set_loop_position({1},{})&&
        !ELYSIA_AUDIO->set_loop_enabled({1},true)&&!ELYSIA_AUDIO->destroy_loop({1}),"unavailable source mutations fail safely");
}

void routing_and_switching(const scene::SceneRuntimeContext& context)
{
    unavailable();
    const auto global=ELYSIA_AUDIO->request_sound("loop",{.loops=-1,.gain=.3,.scope=SoundScope::Global});
    require(bool(global),"explicit global requests remain available");
    State first,second;
    scene::SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<TestScene>(1,std::ref(first));
    manager.register_game_scene<TestScene>(2,std::ref(second));
    manager.start({.target=1});
    require(first.attached&&first.entered&&first.loop,"service available in attach and entry callbacks");
    auto* position_track=track_with_gain(.2);
    require(ELYSIA_AUDIO->set_sound_gain(first.positioned,.4),"one setter routes owned gain through runtime");
    manager.on_update(0); near(MIX_GetTrackGain(position_track),.1);
    require(ELYSIA_AUDIO->set_sound_paused(first.positioned,true),"one setter records owned pause");
    TestScene::current->pause(); ELYSIA_AUDIO->update(2);
    require(ELYSIA_AUDIO->sound_active(first.delayed)&&MIX_TrackPaused(position_track),"owned reservation and voice remain paused");
    require(MIX_TrackPlaying(track_with_gain(.6)),"UI pause exemption survives service forwarding");
    TestScene::current->resume(); manager.on_update(0);
    require(MIX_TrackPaused(position_track),"scene resume retains independent pause");
    require(ELYSIA_AUDIO->set_sound_paused(first.positioned,false),"owned sound explicitly resumes");
    require(ELYSIA_AUDIO->set_listener_position({20,0}),"listener setter forwards"); manager.on_update(0);
    near(MIX_GetTrackGain(position_track),.4);
    require(ELYSIA_AUDIO->listener_position()->x==20,"listener query returns a snapshot");
    require(ELYSIA_AUDIO->set_loop_position(*first.loop,{100,0})&&!ELYSIA_AUDIO->loop_state(*first.loop)->voice,"loop distance release through facade");
    require(ELYSIA_AUDIO->set_loop_gain(*first.loop,.2)&&ELYSIA_AUDIO->set_loop_enabled(*first.loop,false),"source settings forward");
    const auto old_loop=*first.loop;
    manager.on_scene_request({.type=scene::SceneRequestType::Switch,.route={.target=2}}); manager.on_update(0);
    require(first.detached&&first.exited&&second.entered,"scene services detach before exit and reattach before entry");
    require(!ELYSIA_AUDIO->sound_active(first.sound)&&!ELYSIA_AUDIO->sound_active(first.positioned)&&
        !ELYSIA_AUDIO->sound_active(first.delayed)&&ELYSIA_AUDIO->sound_active(global->handle),"switch cleans scene requests while global sound survives");
    require(!ELYSIA_AUDIO->loop_state(old_loop)&&!ELYSIA_AUDIO->destroy_loop(old_loop)&&
        !ELYSIA_AUDIO->set_loop_position(old_loop,{}),"previous-scene logical handle cannot touch new scene");
    require(!ELYSIA_AUDIO->set_sound_gain(first.sound,0)&&!ELYSIA_AUDIO->set_sound_paused(first.sound,true),"old ordinary handles rejected");
    TestScene::current->pause();
    require(ELYSIA_AUDIO->set_listener_position({7,8}),"listener update accepted");
    require(ELYSIA_AUDIO->clear_scene_sounds(),"owned clear forwards");
    require(!ELYSIA_AUDIO->loop_state(*second.loop)&&ELYSIA_AUDIO->listener_position()->x==7,"owned clear invalidates handles and retains listener");
    const auto paused=ELYSIA_AUDIO->request_sound("loop",{.loops=-1,.gain=.45});
    require(paused->handle&&MIX_TrackPaused(track_with_gain(.45)),"owned clear retains scene pause");
    const auto previous_loop=*second.loop;
    manager.on_scene_request({.type=scene::SceneRequestType::Switch,
        .route={.target=2,.reload_mode=scene::SceneReloadMode::Reset}}); manager.on_update(0);
    require(second.loop&&*second.loop!=previous_loop&&!ELYSIA_AUDIO->loop_state(previous_loop)&&
        !ELYSIA_AUDIO->sound_active(paused->handle)&&ELYSIA_AUDIO->listener_position()->x==0,
        "reset rebinds clean runtime and invalidates previous ownership");
    require(ELYSIA_AUDIO->set_sound_gain(global->handle,.25),"global control remains available");
    require(manager.shutdown(),"normal manager shutdown succeeds"); unavailable();
    require(ELYSIA_AUDIO->stop_sound(global->handle),"global cleanup succeeds");
}

void failures(const scene::SceneRuntimeContext& context)
{
    for(int mode=0;mode<4;++mode)
    {
        State state;
        state.fail_attach=mode==0;state.fail_enter=mode==1;state.fail_detach=mode==2;state.fail_exit=mode==3;
        scene::SceneManager manager; manager.initialize(context);manager.register_game_scene<TestScene>(1,std::ref(state));
        manager.start({.target=1}); (void)manager.shutdown();
        require(!ELYSIA_AUDIO->scene_audio_available()&&!ELYSIA_AUDIO->sound_active(state.sound),"exception paths unbind facade and stop owned sounds");
        if(mode>=2) require(state.detached&&state.exited,"teardown callbacks cannot use detached runtime");
    }
    State first,second;
    {
        TestScene a(first),b(second);
        unavailable();
        Access::attach(a);
        bool caught=false;try{Access::attach(b);}catch(const std::exception&){caught=true;}
        require(caught&&second.detach_calls==0&&ELYSIA_AUDIO->scene_audio_available()&&ELYSIA_AUDIO->set_listener_position({3,4}),
            "failed second attachment preserves first runtime without running an unentered user's cleanup hook");
        // Destruction without explicit detach also drops the service's borrowed pointer.
    }
    unavailable();
    {
        TestScene a(first); Access::attach(a); Access::enter(a);
        const auto old_loop=*first.loop;
        ELYSIA_AUDIO->shutdown(); require(ELYSIA_AUDIO->initialize({}),"audio reinitializes while scene remains attached");
        require(ELYSIA_AUDIO->scene_audio_available()&&!ELYSIA_AUDIO->loop_state(old_loop)&&
            !ELYSIA_AUDIO->sound_active(first.sound),"device lifecycle clears source ownership without dangling binding");
        Access::detach(a);Access::exit(a);
    }
    unavailable();
}
}

int main()
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy"); require(SDL_Init(SDL_INIT_AUDIO),"SDL starts");
    require(tests::open_test_mixer()&&ELYSIA_AUDIO->initialize({}),"audio initializes");
    auto* resources=elysia::resources::ResourceManager::instance();
    require(resources->load_sound({"loop",std::filesystem::path(ELYSIA_SOURCE_DIR)/"assets/audio/showcase/loop_a.wav",{}}).has_value(),"fixture loads");
    io::ContentRegistry registry;scene::SceneRuntimeContext context(nullptr,registry,1280,720);
    routing_and_switching(context);failures(context);
    ELYSIA_AUDIO->shutdown();resources->clear();tests::close_test_mixer();SDL_Quit();
}
