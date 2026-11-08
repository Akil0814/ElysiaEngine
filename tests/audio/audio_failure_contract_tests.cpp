#define SDL_MAIN_HANDLED
#include "engine/audio/audio_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/scene/scene.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_checkbox.h"
#include "engine/ui/widgets/ui_radio_button.h"
#include "engine/ui/widgets/ui_slider.h"
#include "engine/tools/logger.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <sstream>
#include <iostream>

using namespace elysia;
using namespace elysia::audio;
using namespace std::chrono_literals;
using tests::require;

namespace
{
struct LogCapture
{
    std::ostringstream output;
    std::streambuf* previous=std::clog.rdbuf(output.rdbuf());
    ~LogCapture() { std::clog.rdbuf(previous); }
};
template<class T> void error_is(const AudioResult<T>& result,AudioError code)
{
    require(!result && result.error().code==code,"operation returns its specific error");
    require(!result.error().diagnostic.message.empty(),"failure retains a diagnostic");
}
AudioResult<void> injected()
{
    return std::unexpected(audio_failure(AudioError::BackendFailure,"Injected backend failure"));
}

void scheduler_failures()
{
    SoundPlaybackScheduler scheduler;
    std::map<int,double> voices;
    bool fail_start=false,fail_stop=false,fail_gain=false;
    int stop_calls=0;
    const auto playing=[&](int channel){return voices.contains(channel);};
    const auto start=[&](std::string_view,int,SoundGroup,double gain)->AudioResult<int>{
        if(fail_start) return std::unexpected(injected().error());
        int channel=0;while(voices.contains(channel)) ++channel;
        voices[channel]=gain;return channel;
    };
    const auto stop=[&](int channel)->AudioResult<void>{
        ++stop_calls;if(fail_stop) return injected();
        voices.erase(channel);return {};
    };
    const auto gain=[&](int channel,SoundGroup,double value)->AudioResult<void>{
        if(fail_gain) return injected();voices.at(channel)=value;return {};
    };
    error_is(scheduler.request_sound("",{},start,playing,stop),AudioError::InvalidArgument);
    error_is(scheduler.request_sound("x",{.loops=-2},start,playing,stop),AudioError::InvalidArgument);
    require(scheduler.set_group_config(SoundGroup::Ui,{.max_simultaneous=1,.cooldown=100ms}),"config accepted");
    const auto old=scheduler.request_sound("x",{.group=SoundGroup::Ui,.fade_in=1s,.gain=.5},start,playing,stop);
    require(old,"initial sound starts");
    error_is(scheduler.request_sound("x",{.group=SoundGroup::Ui},start,playing,stop),AudioError::CooldownActive);
    error_is(scheduler.request_sound("y",{.group=SoundGroup::Ui},start,playing,stop),AudioError::GroupLimitReached);
    error_is(scheduler.set_group_config(SoundGroup::Ui,{.max_simultaneous=5}),AudioError::InvalidArgument);
    require(scheduler.group_config(SoundGroup::Ui)->max_simultaneous==1,"invalid config preserves old value");
    error_is(scheduler.group_config(SoundGroup::Count),AudioError::InvalidArgument);
    require(scheduler.set_group_config(SoundGroup::Ui,{.max_simultaneous=1,.overflow_policy=SoundOverflowPolicy::ReplaceOldest}),"replace enabled");
    fail_stop=true;
    error_is(scheduler.request_sound("y",{.group=SoundGroup::Ui},start,playing,stop),AudioError::BackendFailure);
    require(scheduler.is_active(old->handle,playing)&&voices.size()==1,"failed replacement stop preserves old voice");
    fail_stop=false;
    fail_gain=true;
    error_is(scheduler.set_gain(old->handle,.8,playing,gain),AudioError::BackendFailure);
    scheduler.update(.5,start,playing,stop,gain);
    require(voices.at(0)==0,"failed fade write does not advance committed fade");
    fail_gain=false;
    scheduler.update(.5,start,playing,stop,gain);
    require(voices.at(0)==.25,"failed gain change preserves original gain and fade timeline");
    error_is(scheduler.stop_sound(0,playing,stop),AudioError::InvalidHandle);

    auto second=scheduler.request_sound("other",{},start,playing,stop);
    require(second,"other group starts");
    stop_calls=0;fail_stop=true;
    auto cleanup=scheduler.stop_all_sounds(playing,stop);
    error_is(cleanup,AudioError::BackendFailure);
    require(stop_calls==2&&!cleanup.error().diagnostic.entries.empty(),"cleanup attempts every voice and aggregates errors");
    fail_stop=false;require(scheduler.stop_all_sounds(playing,stop),"cleanup retries successfully");

    // Backend channel exhaustion differs from the requesting group's limit.
    const SoundPlaybackScheduler::StartSoundCallback exhausted=[](std::string_view,int,SoundGroup,double)->AudioResult<int>{
        return std::unexpected(audio_failure(AudioError::ChannelLimitReached,"Native pool exhausted"));
    };
    error_is(scheduler.request_sound("pool",{},exhausted,playing,stop),AudioError::ChannelLimitReached);
    fail_start=true;
    error_is(scheduler.request_sound("bad",{},start,playing,stop),AudioError::BackendFailure);
    auto reservation=scheduler.request_sound("deferred",{.start_delay=100ms},start,playing,stop);
    require(reservation&&reservation->status==SoundRequestStatus::Scheduled,"reservation accepted before backend start");
    require(scheduler.set_paused(reservation->handle,true,playing,{}),"reservation can pause without a native track");
    int failures=0;
    const auto failed=[&](const AudioFailure& failure,SoundHandle handle){
        ++failures;require(failure.code==AudioError::BackendFailure&&handle==reservation->handle,"deferred failure carries code and handle");
        bool key=false;for(const auto& entry:failure.diagnostic.entries) key|=entry.subject_key=="deferred";
        require(key,"deferred failure carries resource key");
    };
    scheduler.update(10,start,playing,stop,gain,failed);
    require(failures==0&&scheduler.is_active(reservation->handle,playing),"paused reservation neither starts nor fails");
    require(scheduler.set_paused(reservation->handle,false,playing,{}),"reservation resumes");
    scheduler.update(.11,start,playing,stop,gain,failed);
    scheduler.update(10,start,playing,stop,gain,failed);
    require(failures==1&&!scheduler.is_active(reservation->handle,playing),"failed reservation is removed and reported once");
    require(scheduler.set_group_config(SoundGroup::Ui,{.max_simultaneous=0,.overflow_policy=SoundOverflowPolicy::ReplaceOldest}),"zero capacity accepted");
    fail_start=false;
    error_is(scheduler.request_sound("zero",{.group=SoundGroup::Ui},start,playing,stop),AudioError::GroupLimitReached);
}

void music_failures()
{
    MusicPlaybackController music;
    bool playing=false,fail_start=false,fail_gain=false;
    double gain=1;
    std::string key;
    MusicPlaybackController::Backend backend{
        [&](std::string_view value,int,double initial)->AudioResult<void>{
            if(fail_start) return injected();key=value;gain=initial;playing=true;return {};
        },
        [&]()->AudioResult<void>{playing=false;return {};},
        [&]{return playing;},
        [&](double value)->AudioResult<void>{if(fail_gain) return injected();gain=value;return {};}
    };
    require(music.play("a",-1,1s,backend),"music starts");
    music.update(.5,backend);require(gain==.5,"music fade advances");
    require(music.transition("b",{.fade_out=1s},backend),"pending music accepted");
    fail_start=true;
    error_is(music.play("bad",-1,0ms,backend),AudioError::BackendFailure);
    require(key=="a"&&gain==.5&&playing,"synchronous start failure preserves current playback");
    fail_gain=true;
    error_is(music.transition("a",{},backend),AudioError::BackendFailure);
    music.update(.5,backend);require(gain==.5,"failed volume write preserves music fade");
    fail_start=false;fail_gain=false;
    music.update(1,backend);require(key=="b","failed synchronous changes preserve accepted transition");
}

void service_errors_and_ui()
{
    LogCapture logs;
    auto* audio=ELYSIA_AUDIO;
    audio->shutdown();
    error_is(audio->request_sound("loop",{.scope=SoundScope::Global}),AudioError::NotInitialized);
    error_is(audio->play_music("music"),AudioError::NotInitialized);
    auto& backend=detail::mixer_backend();
    auto* saved_music=backend.music;
    backend.music=nullptr;
    auto init_failure=audio->initialize({});
    backend.music=saved_music;
    error_is(init_failure,AudioError::BackendFailure);
    require(!audio->is_initialized(),"failed initial backend gain never publishes initialized state");
    require(init_failure.error().diagnostic.message.find("Set music gain")!=std::string::npos,"initialization preserves original backend diagnostic");
    require(audio->initialize({}),"audio starts");
    error_is(audio->request_sound("loop"),AudioError::NoActiveScene);
    error_is(audio->request_sound("missing",{.scope=SoundScope::Global}),AudioError::MissingResource);
    error_is(audio->play_music("missing"),AudioError::MissingResource);
    error_is(audio->stop_sound(0),AudioError::InvalidHandle);
    error_is(audio->set_sound_group_volume(SoundGroup::Count,10),AudioError::InvalidArgument);
    error_is(audio->sound_group_volume(SoundGroup::Count),AudioError::InvalidArgument);
    for(auto options:{SoundPlayOptions{.loops=-2},SoundPlayOptions{.gain=-1},
        SoundPlayOptions{.gain=std::numeric_limits<double>::infinity()},
        SoundPlayOptions{.scope=static_cast<SoundScope>(20)},
        SoundPlayOptions{.pause_policy=static_cast<SoundPausePolicy>(20)},
        SoundPlayOptions{.scope=SoundScope::Global,.spatial=SpatialSoundConfig{{},{10,40,50,1}}},
        SoundPlayOptions{.spatial=SpatialSoundConfig{}}})
        error_is(audio->request_sound("loop",options),AudioError::InvalidArgument);
    error_is(detail::MixerBackend::play(nullptr,nullptr,0),AudioError::BackendFailure);
    require(logs.output.str().empty(),"synchronous errors return diagnostics without duplicate lower-layer logs");
    auto global=audio->request_sound("loop",{.loops=-1,.scope=SoundScope::Global});
    require(global,"explicit global sound works without context");
    require(audio->stop_sound(global->handle),"global sound stops");
    auto* sample=ELYSIA_RESOURCES->find_sound("loop");
    for(auto* track:detail::mixer_backend().tracks)
        require(detail::MixerBackend::play(track,sample,-1),"native pool fills independently of scheduler groups");
    error_is(audio->request_sound("loop",{.scope=SoundScope::Global}),AudioError::ChannelLimitReached);
    for(auto* track:detail::mixer_backend().tracks)
        require(detail::MixerBackend::stop(track),"native pool released");

    // Focus feedback from each built-in widget must still work without a Scene.
    ui::UiButton button(core::Rect{0,0,40,40});button.set_sounds({.focus="loop"});
    ui::UiCheckbox checkbox(core::Rect{0,0,40,40});checkbox.set_sounds({.focus="loop"});
    ui::UiRadioButton radio(core::Rect{0,0,40,40});radio.set_sounds({.focus="loop"});
    ui::UiSlider slider(core::Rect{0,0,40,40});slider.set_sounds({.on_focus="loop"});
    const auto feedback=[&](auto& widget){
        widget.set_focused(true);
        bool live=false;for(auto* track:detail::mixer_backend().tracks) live|=MIX_TrackPlaying(track);
        require(live&&!audio->scene_audio_available(),"widget feedback is explicitly global");
        require(audio->stop_all_sounds(),"widget feedback cleanup succeeds");
    };
    feedback(button);feedback(checkbox);feedback(radio);feedback(slider);

    struct DefaultScene final : scene::Scene
    {
        void on_enter(const scene::ScenePayload&) override {}
        void on_exit() override {}
        void on_reset() override {}
    } scene;
    scene::SceneTestAccess::attach(scene);
    error_is(audio->set_listener_position({NAN,0}),AudioError::InvalidArgument);
    require(audio->listener_position()->x==0,"invalid listener update preserves position");
    error_is(audio->create_loop({.key="loop"}),AudioError::InvalidArgument);
    error_is(audio->loop_state({0}),AudioError::InvalidHandle);
    auto missing=audio->create_loop({.key="missing.loop",.distance={10,40,50,1}});
    require(missing,"missing resource still creates logical loop");
    const auto first_log=logs.output.str();
    require(first_log.find("AUDIO-MissingResource")!=std::string::npos,"logical acquisition records named error");
    for(int i=0;i<40;++i) scene::SceneTestAccess::audio_runtime(scene).update(.01);
    require(logs.output.str()==first_log,"unchanged missing-loop episode never logs every frame");
    scene::SceneTestAccess::detach(scene);
    logs.output.str({});
    auto delayed=audio->request_sound("loop",{.start_delay=100ms,.scope=SoundScope::Global});
    require(delayed,"deferred resource test accepted");
    resources::ResourceManager::instance()->clear();
    audio->update(.11);
    const auto due_log=logs.output.str();
    require(due_log.find("AUDIO-MissingResource")!=std::string::npos&&due_log.find("loop")!=std::string::npos&&
        due_log.find(std::to_string(delayed->handle))!=std::string::npos,"actual due diagnostic retains code, resource and handle");
    audio->update(1);
    require(logs.output.str()==due_log&&!audio->sound_active(delayed->handle),"actual failed reservation logs once and retires");
    audio->shutdown();
}
}

int main()
{
    require(tools::Logger::instance()->configure({.file_mode=tools::LogFileMode::Disabled,
        .console_color_mode=tools::ConsoleColorMode::Never}),"test logging configured");
    scheduler_failures();music_failures();
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");require(SDL_Init(SDL_INIT_AUDIO),"SDL starts");
    require(tests::open_test_mixer(),"dummy mixer starts");
    auto* resources=resources::ResourceManager::instance();
    require(resources->load_sound({"loop",std::filesystem::path(ELYSIA_SOURCE_DIR)/"assets/audio/showcase/loop_a.wav",{}}),"fixture loads");
    service_errors_and_ui();
    resources->clear();tests::close_test_mixer();SDL_Quit();
}
