#define SDL_MAIN_HANDLED
#include "engine/audio/audio_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>

using namespace elysia::audio;
using namespace std::chrono_literals;
using elysia::tests::require;

namespace
{
void near(double a,double b) { require(std::abs(a-b) < 1e-6,"composed gain matches"); }

void scheduler_controls()
{
    SoundPlaybackScheduler scheduler;
    struct Voice { double gain; bool paused = false; };
    std::map<int,Voice> voices;
    bool fail_pause = false;
    int starts = 0;
    const auto start = [&](std::string_view,int,SoundGroup,double gain) {
        int channel=0; while(voices.contains(channel)) ++channel;
        voices[channel]={gain}; ++starts; return channel;
    };
    const auto playing = [&](int c) { return voices.contains(c); };
    const auto stop = [&](int c) { voices.erase(c); };
    const auto volume = [&](int c,SoundGroup,double gain) { voices.at(c).gain=gain; };
    const auto pause = [&](int c,bool value) { if(fail_pause) return false; voices.at(c).paused=value; return true; };
    const auto update = [&](double dt) { scheduler.update(dt,start,playing,stop,volume); };
    auto sound=scheduler.request_sound("loop",{.loops=-1,.group=SoundGroup::Ambient,.fade_in=1s,.gain=.4},start,playing,stop);
    require(sound.handle.has_value(),"request starts"); near(voices.at(0).gain,0);
    update(.5); near(voices.at(0).gain,.2);
    fail_pause=true;
    require(!scheduler.set_paused(*sound.handle,true,playing,pause),"failed backend pause rejected");
    update(.25); near(voices.at(0).gain,.3);
    fail_pause=false;
    require(scheduler.set_paused(*sound.handle,true,playing,pause),"pause accepted");
    update(4); near(voices.at(0).gain,.3);
    require(scheduler.is_active(*sound.handle,playing),"paused voice remains active");
    require(scheduler.set_gain(*sound.handle,.8,playing,volume),"paused gain updates"); near(voices.at(0).gain,.6);
    require(!scheduler.set_gain(*sound.handle,-1,playing,volume),"negative gain rejected");
    require(!scheduler.set_gain(*sound.handle,std::numeric_limits<double>::infinity(),playing,volume),"infinite gain rejected");
    require(!scheduler.request_sound("bad",{.gain=std::numeric_limits<double>::quiet_NaN()},start,playing,stop).handle,"NaN request rejected");
    fail_pause=true;
    require(!scheduler.set_paused(*sound.handle,false,playing,pause),"failed backend resume rejected");
    update(1); near(voices.at(0).gain,.6);
    require(voices.at(0).paused,"failed resume preserves pause intent and frozen fade");
    fail_pause=false;
    require(scheduler.stop_sound(*sound.handle,playing,stop,1s)&&voices.empty(),"paused fade stop releases immediately");

    auto pending=scheduler.request_sound("delayed",{.start_delay=1s},start,playing,stop);
    require(pending.handle&&scheduler.is_active(*pending.handle,playing),"pending request active");
    update(.4); require(scheduler.set_paused(*pending.handle,true,playing,pause),"pending pause accepted");
    update(10); require(voices.empty(),"paused reservation never starts");
    require(scheduler.set_gain(*pending.handle,.25,playing,volume),"pending gain accepted");
    require(scheduler.set_paused(*pending.handle,false,playing,pause),"pending resume accepted");
    update(.59); require(voices.empty(),"remaining delay preserved");
    update(.02); require(voices.size()==1,"reservation starts after remaining delay"); near(voices.at(0).gain,.25);
    const auto old=*pending.handle;
    scheduler.reset(); voices.clear();
    const auto fresh=scheduler.request_sound("fresh",{},start,playing,stop);
    require(fresh.handle&&*fresh.handle!=old&&!scheduler.set_gain(old,0,playing,volume),"reset never reuses a handle");
    SoundPlaybackScheduler other;
    const auto foreign=other.request_sound("foreign",{},start,playing,stop);
    require(foreign.handle&&*foreign.handle!=*fresh.handle&&!scheduler.is_active(*foreign.handle,playing),"cross scheduler handles rejected");
    require(starts==4,"invalid requests never started a voice");
}

void mixer_controls()
{
    auto* audio=AudioService::instance();
    require(audio->initialize({}),"service initializes");
    auto first=audio->request_sound("loop",{.loops=-1,.group=SoundGroup::Ambient,.gain=.5});
    require(first.handle.has_value(),"loop starts");
    auto* track=detail::mixer_backend().tracks[0]; near(MIX_GetTrackGain(track),.5);
    const auto deadline=SDL_GetTicks()+1500;
    while(MIX_GetTrackPlaybackPosition(track)==0 && SDL_GetTicks()<deadline) SDL_Delay(5);
    require(MIX_GetTrackPlaybackPosition(track)>0,"dummy playback advances");
    require(audio->set_sound_paused(*first.handle,true),"track pauses");
    const auto position=MIX_GetTrackPlaybackPosition(track);
    SDL_Delay(60); audio->update(1);
    require(MIX_TrackPaused(track)&&MIX_GetTrackPlaybackPosition(track)==position,"paused track does not advance");
    auto second=audio->request_sound("loop",{.loops=-1});
    require(second.handle&&audio->sound_active(*first.handle)&&MIX_TrackPaused(track),"new voice cannot steal paused track");
    audio->set_master_volume(50); audio->set_sound_volume(40); audio->set_sound_group_volume(SoundGroup::Ambient,50);
    near(MIX_GetTrackGain(track),.05);
    require(audio->set_sound_gain(*first.handle,.25),"instance gain changes"); near(MIX_GetTrackGain(track),.025);
    require(audio->set_sound_gain(*first.handle,std::numeric_limits<double>::max())&&std::isfinite(MIX_GetTrackGain(track)),"finite double gain does not overflow the backend float");
    require(audio->set_sound_gain(*first.handle,.25),"normal gain restored");
    require(audio->set_sound_paused(*first.handle,false),"track resumes");
    require(MIX_GetTrackPlaybackPosition(track)>=position,"resume preserves playback position");
    audio->set_sound_paused(*first.handle,true);
    require(audio->stop_sound(*first.handle,1s)&&!audio->sound_active(*first.handle),"paused SDL track stops without waiting for fade");
    const auto stale=*second.handle;
    audio->shutdown(); require(audio->initialize({}),"service restarts");
    auto fresh=audio->request_sound("loop",{.loops=-1});
    require(fresh.handle&&*fresh.handle!=stale&&!audio->set_sound_paused(stale,true)&&!audio->set_sound_gain(stale,0),"old service handles cannot affect fresh tracks");
    audio->shutdown();
}
}

int main()
{
    scheduler_controls();
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
    require(SDL_Init(SDL_INIT_AUDIO),"SDL initializes");
    require(elysia::tests::open_test_mixer(),"mixer opens");
    auto* resources=elysia::resources::ResourceManager::instance();
    require(resources->load_sound({"loop",std::filesystem::path(ELYSIA_SOURCE_DIR)/"assets/audio/showcase/loop_a.wav",{}}).has_value(),"fixture loads");
    mixer_controls();
    resources->clear(); elysia::tests::close_test_mixer(); SDL_Quit();
}
