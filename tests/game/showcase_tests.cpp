#define SDL_MAIN_HANDLED
#include "game/showcase/audio/audio_showcase_scene.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "game/showcase/shared/showcase_catalog.h"
#include "game/showcase/scenarios/showcase_scenario.h"
#include "game/showcase/physics/box2d_lab_scene.h"
#include "game/showcase/ui/hud_demo_state.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_slider.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/input_snapshot_builder.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"
#include <filesystem>
#include <set>
#include <stdexcept>
namespace {
using elysia::tests::require;
elysia::ui::UiElement* find(elysia::ui::UiElement& element,const std::function<bool(elysia::ui::UiElement&)>& predicate)
{
    if(predicate(element))return &element;
    if(auto* host=dynamic_cast<elysia::ui::UiChildHost*>(&element))
        for(std::size_t i=0;i<host->child_count();++i)if(auto* value=find(*host->child_at(i),predicate))return value;
    return nullptr;
}
void click(elysia::scene::Scene& scene,const char* key)
{
    elysia::scene::SceneTestAccess::update(scene,0);
    auto* root=elysia::scene::SceneTestAccess::ui_root(scene,0);
    auto* element=find(*root,[key](auto& element){auto* b=dynamic_cast<elysia::ui::UiButton*>(&element);return b && b->text_content().value==key;});
    require(element!=nullptr,"showcase action exists");
    auto* scroll=dynamic_cast<elysia::ui::UiScrollContainer*>(find(*root,[](auto& value){return dynamic_cast<elysia::ui::UiScrollContainer*>(&value)!=nullptr;}));
    if(scroll)scroll->ensure_visible(element->screen_rect());
    elysia::scene::SceneTestAccess::update(scene,0);
    const auto center=element->presentation_screen_rect().center();
    for(auto phase:{elysia::input::RawInputEventType::ControlPressed,elysia::input::RawInputEventType::ControlReleased})
        elysia::scene::SceneTestAccess::route_input(scene,elysia::tests::events_snapshot({{
            .control=elysia::input::RawInputControl::MouseLeft,.type=phase,.device=elysia::input::InputDevice::Mouse,
            .mouse_x=int(center.x),.mouse_y=int(center.y)}}));
}
void catalog()
{
    using namespace example::showcase;
    const std::array<elysia::scene::SceneKey,8> order{10,14,13,4,11,15,5,16};
    std::set<elysia::scene::SceneKey> keys;
    for(std::size_t i=0;i<kShowcaseEntries.size();++i){require(kShowcaseEntries[i].key==order[i],"module order is stable");require(keys.insert(order[i]).second,"module keys are unique");}
    int physical=0,gameplay=0,stress=0;
    for(const auto& entry:scenarios::showcase_scenarios()){
        if(entry.category==scenarios::ScenarioCategory::Gameplay){++gameplay;require(entry.scene==17,"combat checks use the Gameplay verification scene");}
        else {require(entry.scene==12,"physics checks use the physics verification scene");if(entry.category==scenarios::ScenarioCategory::Stress)++stress;else ++physical;}
    }
    require(physical==10 && gameplay==3 && stress==3,"all existing simulation cases remain available");
    ui::HudDemoState state;state.cast();state.paused=true;state.advance(5);state.use(0);require(state.cooldown==3 && state.item_counts[0]==8,"pause freezes example actions");state.paused=false;state.advance(3);state.use(1);require(state.cooldown==0 && state.item_counts[1]==2,"example resumes correctly");
}
}
int main()
{
    catalog();
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
    require(SDL_Init(SDL_INIT_AUDIO),"SDL audio initializes");
    require(elysia::tests::open_test_mixer(),"dummy mixer opens");
    auto* resources=elysia::resources::ResourceManager::instance();
    const std::filesystem::path root=ELYSIA_SOURCE_DIR;
    require(resources->load_sound({"showcase.tone",root/"assets/audio/showcase/tone.wav",{}}).has_value(),"generated effect loads");
    for(const char* name:{"a","b"}){
        const auto key=std::string("showcase.loop_")+name;
        require(resources->load_music(elysia::resources::MusicLoadRequest{key,root/("assets/audio/showcase/loop_"+std::string(name)+".wav"),{}}).has_value(),"generated music loads");
    }
    auto* audio=elysia::audio::AudioService::instance();
    const elysia::audio::AudioSettings saved{43,51,67};
    require(audio->initialize(saved),"audio service initializes");
    audio->set_sound_group_volume(elysia::audio::SoundGroup::Gameplay,39);
    require(audio->set_sound_group_config(elysia::audio::SoundGroup::Gameplay,{.max_simultaneous=3}),"group config sets");
    {
        elysia::io::ContentRegistry registry;elysia::scene::SceneRuntimeContext context(nullptr,registry,1280,720);
        example::scene::AudioShowcaseScene scene;elysia::scene::SceneTestAccess::bind(scene,context);
        elysia::scene::SceneTestAccess::enter(scene,example::scene::ShowcaseEnterPayload{{.target=1}});
        auto* ui_root=elysia::scene::SceneTestAccess::ui_root(scene,0);
        auto* slider=dynamic_cast<elysia::ui::UiSlider*>(find(*ui_root,[](auto& element){return dynamic_cast<elysia::ui::UiSlider*>(&element)!=nullptr;}));
        require(slider!=nullptr,"audio controls expose volume");slider->set_value(12);require(audio->settings().master_volume==12,"volume action applies immediately");
        const auto unrelated=audio->request_sound("showcase.tone",{.loops=-1,.group=elysia::audio::SoundGroup::Extra,.start_delay=std::chrono::milliseconds(700)});
        require(unrelated.handle.has_value(),"external delayed sound schedules");
        click(scene,"showcase.audio.delay");
        click(scene,"showcase.audio.loop");
        click(scene,"showcase.audio.music_a");
        audio->update(0.1);
        require(elysia::tests::music_playing(),"music action starts playback");
        elysia::scene::SceneTestAccess::exit(scene);
        require(audio->settings()==saved && audio->sound_group_volume(elysia::audio::SoundGroup::Gameplay)==39,"exit restores audio volumes");
        require(audio->sound_group_config(elysia::audio::SoundGroup::Gameplay).max_simultaneous==3,"exit restores group config");
        require(!elysia::tests::music_playing(),"exit stops owned music");
        audio->update(1);
        int active=0;for(std::size_t i=0;i<elysia::audio::kSoundChannelCount;++i)if(elysia::tests::sound_playing(int(i)))++active;
        require(active==1,"exit cancels only owned pending and active sounds");
        require(audio->stop_sound(*unrelated.handle),"external delayed request survives showcase exit");
    }
    audio->shutdown();
    {
        elysia::io::ContentRegistry registry;elysia::scene::SceneRuntimeContext context(nullptr,registry,1280,720);
        example::scene::AudioShowcaseScene scene;elysia::scene::SceneTestAccess::bind(scene,context);
        elysia::scene::SceneTestAccess::enter(scene,example::scene::ShowcaseEnterPayload{{.target=1}});
        auto* root=elysia::scene::SceneTestAccess::ui_root(scene,0);
        auto* action=find(*root,[](auto& value){auto* button=dynamic_cast<elysia::ui::UiButton*>(&value);return button && button->text_content().value=="showcase.audio.once";});
        require(action && !static_cast<elysia::ui::UiButton*>(action)->is_enabled(),"uninitialized audio disables playback");
        auto* volume=find(*root,[](auto& value){return dynamic_cast<elysia::ui::UiSlider*>(&value)!=nullptr;});
        require(volume && !static_cast<elysia::ui::UiSlider*>(volume)->is_enabled(),"uninitialized audio disables volume controls");
        elysia::scene::SceneTestAccess::exit(scene);
    }
    resources->clear();elysia::tests::close_test_mixer();SDL_Quit();
}
