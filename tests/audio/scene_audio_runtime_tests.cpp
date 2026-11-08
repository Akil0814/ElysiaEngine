#define SDL_MAIN_HANDLED
#include "engine/scene/scene.h"
#include "engine/audio/audio_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"
#include <cmath>
#include <filesystem>
#include <limits>

using namespace elysia::audio;
using namespace elysia::scene;
using namespace std::chrono_literals;
using elysia::tests::require;

namespace
{
const SoundDistanceConfig distance{10,40,50,2};
void near(double a,double b) { require(std::abs(a-b)<1e-6,"spatial gain matches"); }
MIX_Track* voice_track(SoundHandle handle)
{
    // Test fixtures have one loop, so identify its SDL track by the non-null attachment.
    require(ELYSIA_AUDIO->sound_active(handle),"voice handle live");
    for(auto* track:detail::mixer_backend().tracks)
        if(MIX_TrackPlaying(track)||MIX_TrackPaused(track)) return track;
    throw std::runtime_error("No live test track");
}
void restart() { require(ELYSIA_AUDIO->initialize({}),"audio restarts"); }

void spatial_sources()
{
    near(sound_distance_gain(0,distance),1); near(sound_distance_gain(10,distance),1);
    near(sound_distance_gain(20,distance),.25); near(sound_distance_gain(40,distance),.0625);
    near(sound_distance_gain(40.01,distance),0);
    require(!valid_sound_distance({0,40,50,1})&&!valid_sound_distance({10,5,50,1})&&
        !valid_sound_distance({10,40,30,1})&&!valid_sound_distance({10,40,50,0}),"invalid distance ranges rejected");
    near(sound_distance_gain(std::numeric_limits<double>::infinity(),distance),0);
    restart(); SceneAudioRuntime runtime;
    require(!runtime.create_loop({.key="loop"}),"unconfigured distances rejected");
    require(!runtime.set_listener_position({NAN,0}),"invalid listener rejected");
    auto loop=runtime.create_loop({.key="loop",.position={20,0},.distance=distance,.gain=.5});
    require(loop.has_value(),"logical loop created");
    auto state=runtime.loop_state(*loop);
    require(state&&state->voice&&state->status==LoopSoundStatus::Playing,"loop starts immediately with a voice");
    const auto first=*state->voice;
    auto* track=voice_track(first); near(MIX_GetTrackGain(track),.125);
    require(runtime.set_listener_position({10,0}),"listener moves"); runtime.update(0); near(MIX_GetTrackGain(track),.5);
    require(runtime.set_loop_position(*loop,{55,0}),"loop moves into silent band");
    require(MIX_TrackPaused(track)&&runtime.loop_state(*loop)->status==LoopSoundStatus::Inaudible,"silent band pauses track");
    require(runtime.set_loop_position(*loop,{60,0}),"loop reaches release boundary");
    require(runtime.loop_state(*loop)->voice==first,"release boundary retains existing voice");
    require(runtime.set_loop_position(*loop,{61,0}),"loop moves out of range");
    require(!runtime.loop_state(*loop)->voice&&!ELYSIA_AUDIO->sound_active(first),"beyond release range frees channel");
    runtime.update(.25); require(runtime.set_loop_position(*loop,{10,0}),"loop returns");
    require(runtime.loop_state(*loop)->voice&&runtime.loop_state(*loop)->voice!=first,"returning to audible range starts a new voice");
    require(runtime.set_loop_enabled(*loop,false),"loop disables"); runtime.set_paused(true); runtime.set_paused(false);
    require(runtime.loop_state(*loop)->status==LoopSoundStatus::Disabled&&MIX_TrackPaused(track),"scene resume does not enable a disabled source");
    require(!runtime.set_loop_gain(*loop,-1)&&!runtime.set_loop_position(*loop,{0,NAN}),"invalid changes rejected");
    require(runtime.set_loop_enabled(*loop,true),"source reenabled");
    SceneAudioRuntime other;
    require(!other.destroy_loop(*loop)&&!other.loop_state(*loop),"foreign logical handle rejected");
    runtime.clear();
    require(!runtime.loop_state(*loop)&&!runtime.destroy_loop(*loop),"clear invalidates source handle");
    auto fresh=runtime.create_loop({.key="loop",.distance=distance});
    require(fresh&&*fresh!=*loop,"logical handle not reused after clear");
    runtime.set_paused(true); runtime.clear();
    const auto paused=runtime.request_sound("loop",{.loops=-1});
    require(paused->handle&&MIX_TrackPaused(voice_track(paused->handle)),"manual clear preserves scene pause for future requests");
}

void capacity_and_resources()
{
    restart(); SceneAudioRuntime runtime;
    require(elysia::resources::ResourceService::instance()->has_sound("loop")&&
        !elysia::resources::ResourceService::instance()->has_sound("missing"),"quiet sound presence query");
    require(ELYSIA_AUDIO->set_sound_group_config(SoundGroup::Ambient,{.max_simultaneous=1}),"limit set");
    const auto blocker=ELYSIA_AUDIO->request_sound("loop",{.loops=-1,.group=SoundGroup::Ambient,.scope=SoundScope::Global});
    auto loop=runtime.create_loop({.key="loop",.distance=distance});
    require(loop&&!runtime.loop_state(*loop)->voice&&
        runtime.loop_state(*loop)->last_error->code==AudioError::GroupLimitReached,"no capacity retains logical source and specific error");
    require(ELYSIA_AUDIO->stop_sound(blocker->handle),"capacity blocker stops");
    runtime.update(.24); require(!runtime.loop_state(*loop)->voice,"no retry before quarter second");
    runtime.update(.02); require(runtime.loop_state(*loop)->voice.has_value()&&!runtime.loop_state(*loop)->last_error,"retry acquires released channel and clears error");
    runtime.clear();
    auto missing=runtime.create_loop({.key="late",.distance=distance});
    require(missing&&runtime.loop_state(*missing)->status==LoopSoundStatus::MissingResource&&
        runtime.loop_state(*missing)->last_error->code==AudioError::MissingResource,"missing resource is queryable with reason");
    require(runtime.set_loop_enabled(*missing,false)&&!runtime.loop_state(*missing)->last_error,"disabled source no longer needs playback and clears error");
    require(runtime.set_loop_enabled(*missing,true),"missing logical source can be reenabled");
    for(int i=0;i<30;++i) runtime.update(.01);
    const auto file=std::filesystem::path(ELYSIA_SOURCE_DIR)/"assets/audio/showcase/loop_a.wav";
    require(elysia::resources::ResourceManager::instance()->load_sound({"late",file,{}}).has_value(),"late resource loads");
    runtime.update(0); require(runtime.loop_state(*missing)->voice.has_value()&&!runtime.loop_state(*missing)->last_error,"loaded resource can recover and clear error");
    auto* resources=elysia::resources::ResourceManager::instance();
    resources->clear(); runtime.update(0);
    require(!runtime.loop_state(*missing)->voice&&runtime.loop_state(*missing)->status==LoopSoundStatus::MissingResource,"resource unload retires voice without retaining a native pointer");
    require(resources->load_sound({"loop",file,{}}).has_value(),"main fixture reloads after unload");
    runtime.clear();
    auto shot=runtime.request_sound("loop",{.gain=.8,.spatial=SpatialSoundConfig{{20,0},distance}});
    require(bool(shot),"position sound starts");
    auto* track=voice_track(shot->handle); near(MIX_GetTrackGain(track),.2);
    require(runtime.set_sound_gain(shot->handle,.4),"owned shot gain can change without losing attenuation");
    near(MIX_GetTrackGain(track),.1);
    require(runtime.set_sound_paused(shot->handle,true),"owned shot can pause independently");
    runtime.set_paused(true); runtime.set_paused(false); runtime.update(0);
    require(MIX_TrackPaused(track),"scene resume preserves individual pause");
    require(runtime.set_sound_paused(shot->handle,false),"owned shot resumes explicitly");
    require(!runtime.set_sound_gain(shot->handle,NAN),"owned invalid gain rejected");
    require(runtime.set_listener_position({20,0}),"listener returns to source"); runtime.update(0); near(MIX_GetTrackGain(track),.4);
    MIX_StopTrack(track,0); runtime.update(0); require(runtime.set_listener_position({0,0}),"listener moves after completion"); runtime.update(1);
    require(!ELYSIA_AUDIO->sound_active(shot->handle)&&!MIX_TrackPlaying(track),"finished one-shot never restarts");
}

class ProbeScene final : public Scene
{
public:
    ProbeScene():Scene(SceneRuntimeFeatures{.audio=SceneAudioConfig{}}) {}
    SceneAudioRuntime& test_audio() { return SceneTestAccess::audio_runtime(*this); }
    bool fail_enter=false,fail_exit=false,fail_reset=false,fail_detach=false,fail_attach=false;
    SoundHandle game=0,ui=0,delayed=0;
    void on_enter(const ScenePayload&) override
    {
        game=test_audio().request_sound("loop",{.loops=-1,.group=SoundGroup::Gameplay})->handle;
        ui=test_audio().request_sound("loop",{.loops=-1,.group=SoundGroup::Ui,.pause_policy=SoundPausePolicy::ContinueWhenPaused})->handle;
        delayed=test_audio().request_sound("loop",{.start_delay=1s})->handle;
        if(fail_enter) throw std::runtime_error("enter");
    }
    void on_exit() override { if(fail_exit) throw std::runtime_error("exit"); }
    void on_reset() override { if(fail_reset) { on_enter({}); throw std::runtime_error("reset"); } }
    void on_runtime_attach() override { if(fail_attach) { on_enter({}); throw std::runtime_error("attach"); } }
    void on_runtime_detach() override { if(fail_detach) { on_enter({}); throw std::runtime_error("detach"); } }
    void on_after_update(double) override { require(test_audio().set_listener_position({20,0}),"after-update listener changes"); }
};

template<class F> void throws(F fn) { bool caught=false; try{fn();}catch(const std::exception&){caught=true;} require(caught,"injected lifecycle exception observed"); }
void lifecycle()
{
    restart();
    using Access=SceneTestAccess;
    ProbeScene first,second;
    Access::enter(first); Access::enter(second);
    require(!second.test_audio().set_sound_paused(first.game,true)&&
        !second.test_audio().set_sound_gain(first.game,0),"owned sound setters reject another scene's handle");
    first.pause();
    require(MIX_TrackPaused(detail::mixer_backend().tracks[0])&&MIX_TrackPlaying(detail::mixer_backend().tracks[1]),"scene pause keeps UI voice playing");
    ELYSIA_AUDIO->update(2);
    require(ELYSIA_AUDIO->sound_active(first.delayed),"scene paused delayed request remains pending");
    first.resume();
    ELYSIA_AUDIO->update(.9); require(ELYSIA_AUDIO->sound_active(first.delayed),"delayed wait resumes");
    auto spatial=first.test_audio().create_loop({.key="loop",.distance=distance});
    Access::update(first,0);
    require(first.test_audio().loop_state(*spatial)->status==LoopSoundStatus::Playing,"spatial update follows game after-update listener change");
    Access::exit(first);
    require(!ELYSIA_AUDIO->sound_active(first.game)&&!ELYSIA_AUDIO->sound_active(first.ui)&&
        !ELYSIA_AUDIO->sound_active(first.delayed)&&ELYSIA_AUDIO->sound_active(second.game),"exit cleans only owning scene");
    Access::enter(first); first.fail_exit=true; throws([&]{Access::exit(first);});
    require(!ELYSIA_AUDIO->sound_active(first.game),"throwing exit still cleans sounds");
    first.fail_exit=false; Access::reset(first); first.fail_enter=true;
    throws([&]{Access::enter(first);}); require(!ELYSIA_AUDIO->sound_active(first.game),"failed enter cleans sounds");
    first.fail_enter=false; first.fail_reset=true;
    throws([&]{Access::reset(first);}); require(!ELYSIA_AUDIO->sound_active(first.game),"failed reset cleans sounds created by callback");
    first.fail_reset=false; Access::attach(first); Access::enter(first); Access::exit(first);
    first.fail_detach=true; throws([&]{Access::detach(first);});
    require(!ELYSIA_AUDIO->sound_active(first.game),"failed detach cleans sounds");
    first.fail_detach=false; first.fail_attach=true;
    throws([&]{Access::attach(first);}); require(!ELYSIA_AUDIO->sound_active(first.game),"failed attach rolls back sounds");
    Access::exit(second);
    SoundHandle abandoned=0;
    { ProbeScene scope; Access::enter(scope); abandoned=scope.game; }
    require(!ELYSIA_AUDIO->sound_active(abandoned),"destructor cleans without explicit exit");
}
}

int main()
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy"); require(SDL_Init(SDL_INIT_AUDIO),"SDL starts");
    require(elysia::tests::open_test_mixer(),"mixer starts");
    auto* resources=elysia::resources::ResourceManager::instance();
    require(resources->load_sound({"loop",std::filesystem::path(ELYSIA_SOURCE_DIR)/"assets/audio/showcase/loop_a.wav",{}}).has_value(),"sound loads");
    spatial_sources(); capacity_and_resources(); lifecycle();
    ELYSIA_AUDIO->shutdown(); resources->clear(); elysia::tests::close_test_mixer(); SDL_Quit();
}
