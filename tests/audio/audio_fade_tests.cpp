#include "tests/support/sdl_audio_fixture.h"
#define SDL_MAIN_HANDLED
#include "engine/audio/audio_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

using namespace elysia::audio;
using namespace std::chrono_literals;
using elysia::tests::require;

void near(double actual,double expected)
{
    require(std::abs(actual - expected) < 1e-9,"fade gain must match expected value");
}

struct MusicFixture
{
    MusicPlaybackController controller;
    bool playing = false;
    bool fail = false;
    double gain = 1.0;
    int loops = 0;
    std::vector<std::string> starts;
    MusicPlaybackController::Backend backend{
        [this](std::string_view key,int count,double initial)
        {
            if (fail) return false;
            playing = true; gain = initial; loops = count; starts.emplace_back(key); return true;
        },
        [this] { playing = false; },
        [this] { return playing; },
        [this](double value) { gain = value; }
    };
};

void test_music()
{
    MusicFixture f;
    f.playing = true; f.gain = 0.3;
    f.controller.update(1,f.backend);
    near(f.gain,0.3);
    require(f.playing,"idle controller leaves external music alone");
    require(f.controller.play("a",-1,1s,f.backend),"music starts");
    near(f.gain,0);
    f.controller.update(-1,f.backend);
    f.controller.update(std::numeric_limits<double>::infinity(),f.backend);
    f.controller.update(std::numeric_limits<double>::quiet_NaN(),f.backend);
    near(f.gain,0);
    f.controller.update(0.5,f.backend); near(f.gain,0.5);
    f.controller.update(0.5,f.backend); near(f.gain,1);
    require(f.controller.transition("b",{.fade_out=1s,.fade_in=1s},f.backend),"transition accepted");
    f.controller.update(0.5,f.backend); near(f.gain,0.5);
    require(f.controller.transition("c",{.loops=2,.fade_out=5s,.fade_in=2s},f.backend),"latest target accepted");
    f.controller.update(0.5,f.backend);
    require(f.starts == std::vector<std::string>{"a","c"} && f.loops == 2,"latest target replaces pending without restarting fade");
    near(f.gain,0);
    f.controller.update(1,f.backend); near(f.gain,0.5);
    require(f.controller.transition("d",{.fade_out=1s},f.backend),"fade out during fade in");
    f.controller.update(0.5,f.backend); near(f.gain,0.25);
    require(f.controller.transition("c",{.loops=9,.fade_in=1s},f.backend),"same track recovers");
    f.controller.update(0.5,f.backend); near(f.gain,0.625);
    f.controller.update(0.5,f.backend); near(f.gain,1);
    require(f.starts.size() == 2 && f.loops == 2,"same track preserves position and loops");
    require(f.controller.transition("d",{.fade_out=1s},f.backend),"queue target before stop");
    f.controller.stop(2s,f.backend);
    f.controller.update(0.5,f.backend);
    f.controller.stop(2s,f.backend);
    f.controller.update(0.5,f.backend);
    require(!f.playing && f.starts.size() == 2,"stop clears target and repeated stop preserves fade deadline");

    require(f.controller.transition("e",{.fade_out=5s,.fade_in=1s},f.backend),"idle starts immediately");
    near(f.gain,0);
    f.controller.update(10,f.backend); near(f.gain,1);
    require(f.controller.transition("f",{.fade_out=1s},f.backend),"pending before immediate play");
    require(f.controller.play("g",-1,0ms,f.backend),"immediate play overrides pending");
    f.controller.update(2,f.backend);
    require(f.starts.back() == "g","pending must not return after immediate play");
    require(f.controller.transition("h",{.fade_out=5s,.fade_in=1s},f.backend),"natural completion transition");
    f.playing = false;
    f.controller.update(9,f.backend);
    require(f.starts.back() == "h","natural end starts target"); near(f.gain,0);
    require(f.controller.transition("i",{.fade_out=1s},f.backend),"accept before backend failure");
    f.fail = true;
    f.controller.update(1,f.backend);
    require(!f.playing,"failed asynchronous start leaves idle");
    auto count = f.starts.size(); f.fail = false;
    f.controller.update(1,f.backend);
    require(f.starts.size() == count,"failed request is not retried");
    require(f.controller.play("j",-1,-1ms,f.backend),"negative fade starts immediately"); near(f.gain,1);
    require(f.controller.transition("k",{},f.backend),"zero duration switches immediately");
    require(f.starts.back() == "k","zero transition target starts synchronously");
    f.controller.stop(0ms,f.backend); require(!f.playing,"zero stop is immediate");
}

void test_sounds()
{
    SoundPlaybackScheduler scheduler;
    std::unordered_map<int,double> channels;
    int starts = 0;
    // Reuse channel zero after every stop, to catch stale instance volume writes.
    auto start = [&](std::string_view,int,SoundGroup,double gain) { ++starts; channels[0]=gain; return 0; };
    auto playing = [&](int channel) { return channels.contains(channel); };
    auto stop = [&](int channel) { channels.erase(channel); };
    auto volume = [&](int channel,SoundGroup,double gain) { require(channels.contains(channel),"only live channels receive volume"); channels[channel]=gain; };
    auto update = [&](double dt) { scheduler.update(dt,start,playing,stop,volume); };
    require(scheduler.set_group_config(SoundGroup::Ambient,{.max_simultaneous=1}),"set one slot");
    SoundPlayOptions options{.loops=-1,.group=SoundGroup::Ambient,.start_delay=1s,.fade_in=1s};
    auto pending = scheduler.request_sound("a",options,start,playing,stop);
    update(std::numeric_limits<double>::infinity()); update(std::numeric_limits<double>::quiet_NaN()); update(-1);
    require(starts == 0,"invalid dt does not advance delay");
    update(10); near(channels[0],0);
    update(0.5); near(channels[0],0.5);
    require(scheduler.stop_sound(*pending.handle,playing,stop,1s),"fade out during fade in");
    update(0.5); near(channels[0],0.25);
    options.start_delay=0ms;
    require(scheduler.request_sound("b",options,start,playing,stop).status == SoundRequestStatus::Rejected,"fading sound retains capacity");
    require(scheduler.stop_sound(*pending.handle,playing,stop,5s),"repeat stop accepted");
    update(0.5); require(channels.empty(),"repeat stop does not extend fade");
    require(!scheduler.stop_sound(*pending.handle,playing,stop),"finished handle invalid");
    auto next = scheduler.request_sound("b",options,start,playing,stop);
    near(channels[0],0); update(1); near(channels[0],1);
    require(scheduler.stop_sound(*next.handle,playing,stop,1s),"fade before replacement");
    require(scheduler.set_group_config(SoundGroup::Ambient,{.max_simultaneous=1,.overflow_policy=SoundOverflowPolicy::ReplaceOldest}),"replace config");
    auto replacement = scheduler.request_sound("c",options,start,playing,stop);
    require(replacement.handle && !scheduler.stop_sound(*next.handle,playing,stop),"replaced handle invalid");
    update(0.5); near(channels[0],0.5);
    require(scheduler.stop_sound(*replacement.handle,playing,stop,0ms) && channels.empty(),"immediate stop overrides fade");
    options.start_delay=1s;
    auto cancelled = scheduler.request_sound("cancel",options,start,playing,stop);
    require(scheduler.stop_sound(*cancelled.handle,playing,stop,1s),"pending cancels immediately");
    auto before = starts; update(2); require(starts == before,"cancelled sound never starts");
    options.start_delay=0ms; options.fade_in=-1ms;
    auto natural = scheduler.request_sound("natural",options,start,playing,stop);
    near(channels[0],1); channels.clear(); update(1);
    require(!scheduler.stop_sound(*natural.handle,playing,stop),"natural end prunes handle");
    auto active = scheduler.request_sound("active",options,start,playing,stop);
    options.start_delay=2s;
    auto delayed = scheduler.request_sound("delayed",options,start,playing,stop);
    scheduler.stop_all_sounds(playing,stop,1s); update(1);
    require(channels.empty(),"stop all fades active sound");
    update(1); require(channels.contains(0),"stop all retains delayed requests");
    require(scheduler.stop_sound(*delayed.handle,playing,stop),"delayed retains handle after start");
}

void test_service()
{
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
    require(SDL_Init(SDL_INIT_AUDIO),"SDL audio initializes");
    require(elysia::tests::open_test_mixer(),"dummy mixer opens");
    auto* resources = elysia::resources::ResourceManager::instance();
    const std::filesystem::path root = ELYSIA_SOURCE_DIR;
    require(resources->load_sound({"sound",root / "assets/audio/system/button_click_down.wav",{}}).has_value(),"load sound");
    require(resources->load_music(elysia::resources::MusicLoadRequest{"a",root / "assets/engine/audio/Elysian_Realm.ogg",{}}).has_value(),"load music a");
    require(resources->load_music(elysia::resources::MusicLoadRequest{"b",root / "assets/engine/audio/Elysian_Realm.ogg",{}}).has_value(),"load music b");
    auto* audio = AudioService::instance();
    require(audio->initialize({}),"service initializes");
    const auto sound = audio->request_sound("sound",{.loops=-1,.group=SoundGroup::Ambient,.fade_in=1s});
    require(sound.handle && elysia::tests::sound_gain(0) == 0.0f,"SDL channel begins silent");
    audio->update(0.5); require(elysia::tests::sound_gain(0) == 0.5f,"SDL fade midpoint");
    audio->set_master_volume(50); require(elysia::tests::sound_gain(0) == 0.25f,"master composes with gain");
    audio->set_sound_volume(50); require(elysia::tests::sound_gain(0) == 0.125f,"sound composes with gain");
    audio->set_sound_group_volume(SoundGroup::Ambient,50); require(elysia::tests::sound_gain(0) == 0.0625f,"group composes with gain");
    require(audio->play_music("a",-1,1s),"service music starts");
    require(elysia::tests::music_gain() == 0.0f,"music begins silent");
    audio->update(0.5); require(elysia::tests::music_gain() == 0.25f,"music master times gain");
    audio->set_music_volume(50); require(elysia::tests::music_gain() == 0.125f,"music setting composes with gain");
    require(audio->transition_music("b",{.fade_out=1s,.fade_in=1s}),"service transition accepted");
    require(!audio->transition_music("missing"),"invalid transition rejected");
    require(!audio->play_music("missing"),"invalid immediate play rejected");
    audio->update(1); require(elysia::tests::music_playing() && elysia::tests::music_gain() == 0.0f,"valid pending target survives invalid requests");
    audio->update(0.5); require(elysia::tests::music_gain() == 0.125f,"target fade begins at actual start");
    audio->stop_music(1s); audio->stop_all_sounds(1s);
    audio->update(0.5); require(elysia::tests::music_playing() && elysia::tests::sound_playing(0),"fading SDL voices remain live");
    audio->update(0.5); require(!elysia::tests::music_playing() && !elysia::tests::sound_playing(0),"SDL voices halt at fade completion");
    require(audio->settings().master_volume == 50 && audio->settings().music_volume == 50,"fades preserve user settings");
    require(audio->play_music("a"),"restart before shutdown");
    require(audio->transition_music("b",{.fade_out=1s}),"pending before shutdown");
    audio->shutdown(); require(!elysia::tests::music_playing(),"shutdown halts immediately");
    require(audio->initialize({}),"reinitialize"); audio->update(10);
    require(!elysia::tests::music_playing(),"shutdown cleared pending music");
    require(audio->play_music("a",-1),"music restarts before resource unload");
    require(audio->play_sound("sound",-1),"sound restarts before resource unload");
    resources->clear();
    auto& mixer=elysia::audio::detail::mixer_backend();
    require(MIX_GetTrackAudio(mixer.music)==nullptr && !MIX_TrackPlaying(mixer.music),"resource unload detaches and stops music");
    for (auto* track : mixer.tracks)
        require(MIX_GetTrackAudio(track)==nullptr && !MIX_TrackPlaying(track),"resource unload detaches and stops sound tracks");
    audio->shutdown(); elysia::tests::close_test_mixer(); SDL_Quit();
}

int main()
{
    test_music();
    test_sounds();
    test_service();
}
