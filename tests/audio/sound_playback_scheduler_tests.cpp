#include "engine/audio/audio_service.h"
#include "engine/audio/sound_playback_scheduler.h"
#include "tests/support/test_assertions.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace
{
using elysia::tests::require;
using namespace std::chrono_literals;
using elysia::audio::SoundGroup;
using elysia::audio::SoundGroupConfig;
using elysia::audio::SoundPlayOptions;
using elysia::audio::SoundPlaybackScheduler;
using elysia::audio::SoundRequestStatus;

struct FakeChannels
{
    int next_channel = 0;
    int start_count = 0;
    std::unordered_set<int> playing;
    std::vector<int> stopped_channels;

    int start(std::string_view,int,SoundGroup)
    {
        const int channel = next_channel++;
        playing.insert(channel);
        ++start_count;
        return channel;
    }

    [[nodiscard]] bool is_playing(int channel) const
    {
        return playing.contains(channel);
    }

    void stop(int channel)
    {
        playing.erase(channel);
        stopped_channels.push_back(channel);
    }
};

SoundPlayOptions options_for(SoundGroup group)
{
    SoundPlayOptions options{};
    options.group = group;
    return options;
}

void test_group_limits_and_finished_channel_cleanup()
{
    SoundPlaybackScheduler scheduler;
    FakeChannels channels;
    const auto start = [&channels](std::string_view key,int loops,SoundGroup group) { return channels.start(key,loops,group); };
    const auto playing = [&channels](int channel) { return channels.is_playing(channel); };
    const SoundPlayOptions ui = options_for(SoundGroup::Ui);

    for (int index = 0; index < 4; ++index)
        require(scheduler.request_sound("ui.click",ui,start,playing).status == SoundRequestStatus::Started,
            "UI group must accept sounds until its hard limit");
    require(scheduler.request_sound("ui.click",ui,start,playing).status == SoundRequestStatus::Rejected,
        "UI group must reject sounds over its hard limit");

    channels.playing.erase(0);
    require(scheduler.request_sound("ui.click",ui,start,playing).status == SoundRequestStatus::Started,
        "a finished channel must be reclaimed before checking group limits");
}

void test_configurable_limits_and_global_budget()
{
    SoundPlaybackScheduler scheduler;
    FakeChannels channels;
    const auto start = [&channels](std::string_view key,int loops,SoundGroup group) { return channels.start(key,loops,group); };
    const auto playing = [&channels](int channel) { return channels.is_playing(channel); };

    SoundGroupConfig ui_config{};
    ui_config.max_simultaneous = 2;
    require(scheduler.set_group_config(SoundGroup::Ui,ui_config),"a group limit below its hard limit must be accepted");
    ui_config.max_simultaneous = 5;
    require(!scheduler.set_group_config(SoundGroup::Ui,ui_config),"a group limit above its hard limit must be rejected");

    const auto ui = options_for(SoundGroup::Ui);
    require(scheduler.request_sound("ui.one",ui,start,playing).status == SoundRequestStatus::Started,"first configured UI slot must start");
    require(scheduler.request_sound("ui.two",ui,start,playing).status == SoundRequestStatus::Started,"second configured UI slot must start");
    require(scheduler.request_sound("ui.three",ui,start,playing).status == SoundRequestStatus::Rejected,"configured UI limit must be enforced");

    SoundPlaybackScheduler global_scheduler;
    FakeChannels global_channels;
    const auto global_start = [&global_channels](std::string_view key,int loops,SoundGroup group) { return global_channels.start(key,loops,group); };
    const auto global_playing = [&global_channels](int channel) { return global_channels.is_playing(channel); };
    for (const auto group : { SoundGroup::Ui,SoundGroup::Gameplay,SoundGroup::Ambient,SoundGroup::Extra })
    {
        const SoundPlayOptions options = options_for(group);
        const std::size_t count = elysia::audio::sound_group_hard_limit(group);
        for (std::size_t index = 0; index < count; ++index)
            require(global_scheduler.request_sound("sound." + std::to_string(global_channels.start_count),options,global_start,global_playing).status == SoundRequestStatus::Started,
                "all fixed group budgets must fit inside the global channel budget");
    }
    require(global_channels.start_count == static_cast<int>(elysia::audio::kSoundChannelCount),
        "the four fixed group budgets must consume exactly 24 channels");
}

void test_per_key_cooldown()
{
    SoundPlaybackScheduler scheduler;
    FakeChannels channels;
    const auto start = [&channels](std::string_view key,int loops,SoundGroup group) { return channels.start(key,loops,group); };
    const auto playing = [&channels](int channel) { return channels.is_playing(channel); };

    SoundGroupConfig config{};
    config.cooldown = 100ms;
    require(scheduler.set_group_config(SoundGroup::Ui,config),"valid cooldown config must be accepted");
    const auto ui = options_for(SoundGroup::Ui);
    require(scheduler.request_sound("ui.click",ui,start,playing).status == SoundRequestStatus::Started,"first key request must start");
    require(scheduler.request_sound("ui.click",ui,start,playing).status == SoundRequestStatus::Rejected,"same key must be cooled down");
    require(scheduler.request_sound("ui.focus",ui,start,playing).status == SoundRequestStatus::Started,"different key must not share cooldown");
    scheduler.update(0.1,start,playing);
    require(scheduler.request_sound("ui.click",ui,start,playing).status == SoundRequestStatus::Started,"key cooldown must expire after its interval");
}

void test_group_overflow_policies()
{
    SoundPlaybackScheduler ignore_scheduler;
    FakeChannels ignore_channels;
    const auto ignore_start = [&ignore_channels](std::string_view key,int loops,SoundGroup group) { return ignore_channels.start(key,loops,group); };
    const auto ignore_playing = [&ignore_channels](int channel) { return ignore_channels.is_playing(channel); };
    const auto ignore_stop = [&ignore_channels](int channel) { ignore_channels.stop(channel); };
    const auto ui = options_for(SoundGroup::Ui);
    for (int index = 0; index < 4; ++index)
        require(ignore_scheduler.request_sound("ui.active",ui,ignore_start,ignore_playing,ignore_stop).status == SoundRequestStatus::Started,
            "default overflow policy must fill the UI group");
    require(ignore_scheduler.request_sound("ui.ignored",ui,ignore_start,ignore_playing,ignore_stop).status == SoundRequestStatus::Rejected,
        "default overflow policy must reject a full group");
    require(ignore_channels.stopped_channels.empty(),"ignore policy must not stop any active channel");

    SoundPlaybackScheduler replace_scheduler;
    FakeChannels replace_channels;
    const auto replace_start = [&replace_channels](std::string_view key,int loops,SoundGroup group) { return replace_channels.start(key,loops,group); };
    const auto replace_playing = [&replace_channels](int channel) { return replace_channels.is_playing(channel); };
    const auto replace_stop = [&replace_channels](int channel) { replace_channels.stop(channel); };
    SoundGroupConfig replace_config{};
    replace_config.overflow_policy = elysia::audio::SoundOverflowPolicy::ReplaceOldest;
    require(replace_scheduler.set_group_config(SoundGroup::Ui,replace_config),"replace policy config must be accepted");

    const auto gameplay = options_for(SoundGroup::Gameplay);
    require(replace_scheduler.request_sound("gameplay.active",gameplay,replace_start,replace_playing,replace_stop).status == SoundRequestStatus::Started,
        "other groups must retain their own active channels");
    for (int index = 0; index < 4; ++index)
        require(replace_scheduler.request_sound("ui.active",ui,replace_start,replace_playing,replace_stop).status == SoundRequestStatus::Started,
            "replace policy must fill the target group before replacing");
    require(replace_scheduler.request_sound("ui.newest",ui,replace_start,replace_playing,replace_stop).status == SoundRequestStatus::Started,
        "replace policy must accept a new request for a full group");
    require(replace_channels.stopped_channels.size() == 1 && replace_channels.stopped_channels.front() == 1,
        "replace policy must stop the earliest channel in the same group");
    require(replace_channels.playing.contains(0),"replace policy must not stop another group's channel");

    SoundPlaybackScheduler cooldown_scheduler;
    FakeChannels cooldown_channels;
    const auto cooldown_start = [&cooldown_channels](std::string_view key,int loops,SoundGroup group) { return cooldown_channels.start(key,loops,group); };
    const auto cooldown_playing = [&cooldown_channels](int channel) { return cooldown_channels.is_playing(channel); };
    const auto cooldown_stop = [&cooldown_channels](int channel) { cooldown_channels.stop(channel); };
    SoundGroupConfig cooldown_config{};
    cooldown_config.cooldown = 100ms;
    cooldown_config.overflow_policy = elysia::audio::SoundOverflowPolicy::ReplaceOldest;
    require(cooldown_scheduler.set_group_config(SoundGroup::Ui,cooldown_config),"replace cooldown config must be accepted");
    require(cooldown_scheduler.request_sound("ui.repeat",ui,cooldown_start,cooldown_playing,cooldown_stop).status == SoundRequestStatus::Started,
        "initial cooldown request must start");
    for (int index = 0; index < 3; ++index)
        require(cooldown_scheduler.request_sound("ui.other." + std::to_string(index),ui,cooldown_start,cooldown_playing,cooldown_stop).status == SoundRequestStatus::Started,
            "different keys must fill the group during cooldown testing");
    require(cooldown_scheduler.request_sound("ui.repeat",ui,cooldown_start,cooldown_playing,cooldown_stop).status == SoundRequestStatus::Rejected,
        "cooldown must reject before overflow replacement");
    require(cooldown_channels.stopped_channels.empty(),"cooldown rejection must not replace an active sound");
}

void test_delayed_replace_oldest()
{
    SoundPlaybackScheduler scheduler;
    FakeChannels channels;
    const auto start = [&channels](std::string_view key,int loops,SoundGroup group) { return channels.start(key,loops,group); };
    const auto playing = [&channels](int channel) { return channels.is_playing(channel); };
    const auto stop = [&channels](int channel) { channels.stop(channel); };
    const auto ui = options_for(SoundGroup::Ui);
    SoundGroupConfig config{};
    config.overflow_policy = elysia::audio::SoundOverflowPolicy::ReplaceOldest;
    require(scheduler.set_group_config(SoundGroup::Ui,config),"delayed replace config must be accepted");

    for (int index = 0; index < 4; ++index)
        require(scheduler.request_sound("ui.active",ui,start,playing,stop).status == SoundRequestStatus::Started,
            "replace group must be full before delayed request is due");
    SoundPlayOptions delayed = ui;
    delayed.start_delay = 1ms;
    require(scheduler.request_sound("ui.delayed",delayed,start,playing,stop).status == SoundRequestStatus::Scheduled,
        "delayed request must be scheduled before applying overflow policy");
    scheduler.update(0.001,start,playing,stop);
    require(channels.start_count == 5 && channels.stopped_channels.size() == 1,
        "due delayed request must use the configured replacement policy");
}

void test_sound_handles_and_active_channel_enumeration()
{
    SoundPlaybackScheduler scheduler;
    FakeChannels channels;
    const auto start = [&channels](std::string_view key,int loops,SoundGroup group) { return channels.start(key,loops,group); };
    const auto playing = [&channels](int channel) { return channels.is_playing(channel); };
    const auto stop = [&channels](int channel) { channels.stop(channel); };
    const auto ui = options_for(SoundGroup::Ui);

    const auto direct = scheduler.request_sound("ui.direct",ui,start,playing,stop);
    require(direct.status == SoundRequestStatus::Started && direct.handle,"direct sound must return an active handle");
    require(scheduler.stop_sound(*direct.handle,playing,stop),"active handle must stop its channel");
    require(channels.stopped_channels == std::vector<int>{ 0 },"stopping an active handle must stop its channel exactly once");
    require(!scheduler.stop_sound(*direct.handle,playing,stop),"stopped handle must become invalid");

    SoundPlayOptions delayed = ui;
    delayed.start_delay = 10ms;
    const auto scheduled = scheduler.request_sound("ui.delayed",delayed,start,playing,stop);
    require(scheduled.status == SoundRequestStatus::Scheduled && scheduled.handle,"scheduled sound must return the same public handle type");
    require(scheduler.stop_sound(*scheduled.handle,playing,stop),"pending handle must cancel its request");
    scheduler.update(0.01,start,playing,stop);
    require(channels.start_count == 1,"cancelled pending handle must never start a channel");

    const auto gameplay = options_for(SoundGroup::Gameplay);
    require(scheduler.request_sound("gameplay.active",gameplay,start,playing,stop).status == SoundRequestStatus::Started,
        "active channel enumeration needs a second group");
    const auto ui_active = scheduler.request_sound("ui.active",ui,start,playing,stop);
    require(ui_active.handle.has_value(),"second UI active sound must have a handle");
    std::vector<int> ui_channels;
    scheduler.for_each_active_channel(SoundGroup::Ui,playing,[&ui_channels](int channel) { ui_channels.push_back(channel); });
    require(ui_channels == std::vector<int>{ 2 },"active channel enumeration must return only the requested group");

    SoundPlaybackScheduler replacing_scheduler;
    FakeChannels replacing_channels;
    const auto replacing_start = [&replacing_channels](std::string_view key,int loops,SoundGroup group) { return replacing_channels.start(key,loops,group); };
    const auto replacing_playing = [&replacing_channels](int channel) { return replacing_channels.is_playing(channel); };
    const auto replacing_stop = [&replacing_channels](int channel) { replacing_channels.stop(channel); };
    SoundGroupConfig replace_config{};
    replace_config.max_simultaneous = 1;
    replace_config.overflow_policy = elysia::audio::SoundOverflowPolicy::ReplaceOldest;
    require(replacing_scheduler.set_group_config(SoundGroup::Ui,replace_config),"replacement handle test config must be accepted");
    const auto replaced = replacing_scheduler.request_sound("ui.old",ui,replacing_start,replacing_playing,replacing_stop);
    require(replaced.handle.has_value(),"replaceable sound must return a handle");
    require(replacing_scheduler.request_sound("ui.new",ui,replacing_start,replacing_playing,replacing_stop).status == SoundRequestStatus::Started,
        "replacement policy must start the newer sound");
    require(!replacing_scheduler.stop_sound(*replaced.handle,replacing_playing,replacing_stop),
        "replaced sound handle must become invalid");
}

void test_runtime_sound_group_volumes()
{
    auto* audio = elysia::audio::AudioService::instance();
    audio->set_sound_group_volume(SoundGroup::Ui,-1);
    audio->set_sound_group_volume(SoundGroup::Gameplay,42);
    audio->set_sound_group_volume(SoundGroup::Ambient,101);
    require(audio->sound_group_volume(SoundGroup::Ui) == 0,"group volume must clamp below zero");
    require(audio->sound_group_volume(SoundGroup::Gameplay) == 42,"group volume must preserve in-range values");
    require(audio->sound_group_volume(SoundGroup::Ambient) == 100,"group volume must clamp above 100");
    require(audio->sound_group_volume(SoundGroup::Extra) == 100,"group volumes must remain independent");
}

void test_delayed_requests_cancellation_and_capacity_drop()
{
    SoundPlaybackScheduler scheduler;
    FakeChannels channels;
    const auto start = [&channels](std::string_view key,int loops,SoundGroup group) { return channels.start(key,loops,group); };
    const auto playing = [&channels](int channel) { return channels.is_playing(channel); };

    SoundPlayOptions delayed = options_for(SoundGroup::Gameplay);
    delayed.start_delay = 100ms;
    const auto request = scheduler.request_sound("gameplay.explosion",delayed,start,playing);
    require(request.status == SoundRequestStatus::Scheduled && request.handle,"delayed request must return a cancellable handle");
    scheduler.update(0.099,start,playing);
    require(channels.start_count == 0,"delayed sound must not start before its due time");
    scheduler.update(0.001,start,playing);
    require(channels.start_count == 1,"delayed sound must start at its due time");

    const auto cancelled = scheduler.request_sound("gameplay.cancelled",delayed,start,playing);
    require(cancelled.handle && scheduler.stop_sound(*cancelled.handle,playing,{}),"scheduled request must be cancellable through its handle");
    scheduler.update(0.1,start,playing);
    require(channels.start_count == 1,"cancelled request must never start");

    SoundPlaybackScheduler full_scheduler;
    FakeChannels full_channels;
    const auto full_start = [&full_channels](std::string_view key,int loops,SoundGroup group) { return full_channels.start(key,loops,group); };
    const auto full_playing = [&full_channels](int channel) { return full_channels.is_playing(channel); };
    const auto ui = options_for(SoundGroup::Ui);
    for (int index = 0; index < 4; ++index)
        require(full_scheduler.request_sound("ui.active",ui,full_start,full_playing).status == SoundRequestStatus::Started,"fill UI group before delayed capacity test");
    SoundPlayOptions delayed_ui = ui;
    delayed_ui.start_delay = 1ms;
    require(full_scheduler.request_sound("ui.delayed",delayed_ui,full_start,full_playing).status == SoundRequestStatus::Scheduled,"full group request can wait for its scheduled time");
    full_scheduler.update(0.001,full_start,full_playing);
    require(full_channels.start_count == 4,"due sound must be dropped when its group is full");
}
}

int main()
{
    test_group_limits_and_finished_channel_cleanup();
    test_configurable_limits_and_global_budget();
    test_per_key_cooldown();
    test_group_overflow_policies();
    test_delayed_requests_cancellation_and_capacity_drop();
    test_delayed_replace_oldest();
    test_sound_handles_and_active_channel_enumeration();
    test_runtime_sound_group_volumes();
    std::cout << "sound playback scheduler tests passed\n";
    return EXIT_SUCCESS;
}
