#include "game/showcase/audio/audio_showcase_scene.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "engine/ui/widgets/ui_slider.h"
#include <stdexcept>
namespace example::scene
{
using namespace elysia::audio;
using namespace elysia::ui;
using namespace std::chrono_literals;
AudioShowcaseScene::AudioShowcaseScene()
    : Scene(elysia::scene::SceneRuntimeFeatures{.audio=elysia::scene::SceneAudioConfig{}}) {}
AudioShowcaseScene::~AudioShowcaseScene() { cleanup(); }
void AudioShowcaseScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    resume();
    const auto* entry=elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if (!entry || !elysia::scene::SceneKeys::is_supported(entry->return_route.target)) throw std::logic_error("AudioShowcaseScene requires a valid ShowcaseEnterPayload.");
    _return_route=entry->return_route;
    auto* audio=AudioService::instance();
    if (audio->is_initialized()) {
        _saved=audio->settings();
        for (std::size_t i=0;i<kSoundGroupCount;++i) { auto group=static_cast<SoundGroup>(i); _groups[i]=audio->sound_group_config(group); _volumes[i]=audio->sound_group_volume(group); }
        _captured=true;
    }
    try {
        _window=create_and_add_object<UiWindow>(elysia::core::Rect{0,0,float(runtime_context().logical_width()),float(runtime_context().logical_height())},100);
        _view.build(*_window,"showcase.audio.title","showcase.audio.description",[this]{back();});
        const auto add=[&](const char* key,std::function<void()> action) { auto* button=_view.add_action(key,std::move(action)); button->set_enabled(_captured); };
        add("showcase.audio.once",[this]{play({.group=SoundGroup::Gameplay});});
        add("showcase.audio.delay",[this]{play({.group=SoundGroup::Gameplay,.start_delay=500ms});});
        add("showcase.audio.loop",[this]{play({.loops=-1,.group=SoundGroup::Ambient,.fade_in=400ms});});
        add("showcase.audio.stop",[this]{stop_sounds();report(true);});
        add("showcase.audio.spatial_start",[this]{
            if(_spatial_loop) audio_runtime().destroy_loop(*_spatial_loop);
            _spatial_enabled=true;
            _spatial_loop=audio_runtime().create_loop({.key="showcase.tone",.distance={80,320,480,1}});
            report(_spatial_loop.has_value());
        });
        add("showcase.audio.spatial_toggle",[this]{
            _spatial_enabled=!_spatial_enabled;
            report(_spatial_loop&&audio_runtime().set_loop_enabled(*_spatial_loop,_spatial_enabled));
        });
        add("showcase.audio.scene_pause",[this]{if(is_paused())resume();else pause();report(true);});
        add("showcase.audio.pause_ui",[this]{
            report(audio_runtime().request_sound("showcase.tone",{.group=SoundGroup::Ui},
                elysia::scene::SceneSoundPausePolicy::ContinueWhenPaused).handle.has_value());
        });
        add("showcase.audio.limit",[this] {
            auto* audio=AudioService::instance();
            (void)audio->set_sound_group_config(SoundGroup::Gameplay,{.max_simultaneous=1,.cooldown=500ms,.overflow_policy=SoundOverflowPolicy::IgnoreNew});
            play({.loops=-1,.group=SoundGroup::Gameplay}); play({.group=SoundGroup::Gameplay});
        });
        add("showcase.audio.music_a",[this]{bool ok=AudioService::instance()->play_music("showcase.loop_a",-1,500ms);_owns_music=_owns_music||ok;report(ok);});
        add("showcase.audio.music_b",[this]{bool ok=AudioService::instance()->transition_music("showcase.loop_b",{.loops=-1,.fade_out=500ms,.fade_in=500ms});_owns_music=_owns_music||ok;report(ok);});
        add("showcase.audio.music_stop",[this]{if(_owns_music)AudioService::instance()->stop_music(500ms);report(true);});
        const char* keys[]={"showcase.audio.master","showcase.audio.music_volume","showcase.audio.sound_volume","showcase.audio.group_volume"};
        const int values[]={_saved.master_volume,_saved.music_volume,_saved.sound_volume,_volumes[sound_group_index(SoundGroup::Gameplay)]};
        for (int i=0;i<4;++i) {
            _view.content().add_back(std::make_unique<UiLabel>(elysia::core::Rect{0,0,800,26},0,ui_text_key(keys[i])));
            auto slider=std::make_unique<UiSlider>(elysia::core::Rect{0,0,800,44}); slider->set_range(0,100);slider->set_step(1);slider->set_value(float(values[i]));slider->set_enabled(_captured);
            slider->set_on_value_changed([i](float value){auto* audio=AudioService::instance();const int volume=int(value);if(i==0)audio->set_master_volume(volume);else if(i==1)audio->set_music_volume(volume);else if(i==2)audio->set_sound_volume(volume);else audio->set_sound_group_volume(SoundGroup::Gameplay,volume);});
            _view.content().add_back(std::move(slider));
        }
        _view.content().add_back(std::make_unique<UiLabel>(elysia::core::Rect{0,0,800,26},0,ui_text_key("showcase.audio.listener")));
        auto listener=std::make_unique<UiSlider>(elysia::core::Rect{0,0,800,44});
        listener->set_range(0,560);listener->set_step(10);listener->set_value(0);listener->set_enabled(_captured);
        listener->set_on_value_changed([this](float value){audio_runtime().set_listener_position({value,0});});
        _view.content().add_back(std::move(listener));
        _view.set_status(ui_text_key(_captured?"showcase.audio.ready":"showcase.audio.unavailable"));
    } catch (...) { cleanup(); throw; }
}
void AudioShowcaseScene::report(bool success) { _view.set_status(ui_text_key(success?"showcase.audio.accepted":"showcase.audio.rejected")); }
void AudioShowcaseScene::play(SoundPlayOptions options)
{
    const auto request=audio_runtime().request_sound("showcase.tone",options);
    _view.set_status(ui_text_key(request.status==SoundRequestStatus::Scheduled?"showcase.audio.scheduled":request.status==SoundRequestStatus::Started?"showcase.audio.accepted":"showcase.audio.rejected"));
}
void AudioShowcaseScene::stop_sounds()
{
    const auto listener=audio_runtime().listener_position();
    audio_runtime().clear();
    audio_runtime().set_listener_position(listener);
    audio_runtime().set_paused(is_paused());
    _spatial_loop.reset();_spatial_enabled=true;
}
void AudioShowcaseScene::cleanup()
{
    if (!_captured) return;
    auto* audio=AudioService::instance();
    stop_sounds(); if (_owns_music) audio->stop_music(); _owns_music=false;
    audio->set_master_volume(_saved.master_volume);audio->set_music_volume(_saved.music_volume);audio->set_sound_volume(_saved.sound_volume);
    for(std::size_t i=0;i<kSoundGroupCount;++i){auto group=static_cast<SoundGroup>(i);(void)audio->set_sound_group_config(group,_groups[i]);audio->set_sound_group_volume(group,_volumes[i]);}
    _captured=false;
}
void AudioShowcaseScene::on_exit() { cleanup(); if (_window) { _window->set_active(false); _window->set_visible(false); } }
void AudioShowcaseScene::on_reset() { cleanup();_view.clear();if(_window)_window->destroy();_window=nullptr; }
void AudioShowcaseScene::back() { request_scene_switch(_return_route); }
void AudioShowcaseScene::on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>& events)
{ for(const auto& event:events) if(event.control==elysia::input::RawInputControl::KeyEscape && event.type==elysia::input::RawInputEventType::ControlPressed){consume_input(event);back();return;} }
}
