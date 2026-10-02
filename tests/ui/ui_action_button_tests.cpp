#define SDL_MAIN_HANDLED

#include "engine/ui/widgets/ui_action_button.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/style/ui_theme_manager.h"
#include "engine/scene/scene.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/input_snapshot_builder.h"
#include "tests/support/test_assertions.h"
#include "tests/support/sdl_audio_fixture.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{
using namespace elysia;
using tests::require;
using Phase = ui::UiActionButtonInteractionPhase;
static_assert(!std::is_base_of_v<ui::UiControl,ui::UiActionButton>);

ui::UiInputEvent pointer(ui::UiInputEventType type,int x = 10,int y = 10)
{
    return { .type=type,.device=input::InputDevice::Mouse,.control=input::RawInputControl::MouseLeft,
        .mouse_x=x,.mouse_y=y };
}
void press(ui::UiActionButton& button)
{
    require(button.on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed)),"pointer press must be consumed");
}

void test_interaction_and_external_state()
{
    ui::UiActionButton button(core::Rect{ 0,0,40,40 });
    std::vector<ui::UiActionButtonInteraction> events;
    button.set_on_interaction([&](const auto& event) { events.push_back(event); });
    require(!button.on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed,50,50)),"outside press must pass through");
    require(!button.on_ui_input_event(pointer(ui::UiInputEventType::PointerReleased)),"unowned release must pass through");
    button.set_external_pressed(true);
    button.set_selected(true);
    button.set_overlay_ratio(0.5f);
    require(events.empty() && button.is_pressed(),"external updates must not notify gameplay");
    press(button);
    press(button);
    require(events.size() == 1 && events[0].phase == Phase::Pressed,"duplicate presses must notify once");
    require(!button.on_ui_input_event(pointer(ui::UiInputEventType::MouseMoved,80,80)) && button.is_pointer_pressed(),
        "dragging outside must retain the press without consuming movement");
    require(button.on_ui_input_event(pointer(ui::UiInputEventType::PointerReleased,80,80)),"owned outside release must be consumed");
    require(events.back().phase == Phase::Released && !events.back().clicked && button.is_pressed(),
        "outside release must not click or clear external press");
    press(button);
    button.set_external_pressed(false);
    require(button.is_pressed(),"external release must not clear pointer press");
    (void)button.on_ui_input_event(pointer(ui::UiInputEventType::MouseMoved,80,80));
    (void)button.on_ui_input_event(pointer(ui::UiInputEventType::MouseMoved));
    (void)button.on_ui_input_event(pointer(ui::UiInputEventType::PointerReleased));
    require(events.back().clicked && !button.is_pressed(),"moving back inside must permit a click");
    press(button);
    button.set_enabled(false);
    require(events.back().phase == Phase::Canceled && !button.is_pressed(),"disable must cancel pointer hold");
    const auto count = events.size();
    button.cancel_input_interaction();
    require(events.size() == count && !button.on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed)),
        "cancel must be idempotent and disabled controls must pass through");
    button.set_enabled(true);
    press(button);
    button.reset();
    require(events.size() == count + 1 && !button.is_pressed() && button.is_enabled(),"reset must silently reset all state");
    for (auto device : { input::InputDevice::Keyboard,input::InputDevice::Gamepad })
        require(!button.on_ui_input_event({ .action=ui::UiAction::Confirm,.type=ui::UiInputEventType::ActionPressed,.device=device }),
            "HUD must ignore confirm input");
    require(button.input_capture() == input::InputCapture::None,"HUD must not capture device classes");
    for (float value : { -1.0f,2.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN() })
    {
        button.set_overlay_ratio(value);
        require(button.overlay_ratio() == (value == 2.0f ? 1.0f : 0.0f),"overlay input must be normalized");
    }
    button.set_visible(false);
    require(!button.on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed)),"hidden HUD must pass through");
    button.set_visible(true);
    button.set_active(false);
    require(!button.on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed)),"inactive HUD must pass through");
    button.set_active(true);
    button.destroy();
    require(!button.on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed)),"destroyed HUD must pass through");
}

void test_callback_lifetime_and_batch_cancellation()
{
    for (const auto phase : { Phase::Pressed,Phase::Released,Phase::Canceled })
    {
        ui::UiChildHost host;
        auto owned = std::make_unique<ui::UiActionButton>(core::Rect{ 0,0,40,40 });
        auto* button = owned.get();
        host.add_child(std::move(owned));
        bool called = false;
        button->set_on_interaction([&](const auto& event) {
            if (event.phase == phase) { called = true; host.clear_children(); }
        });
        press(*button);
        if (phase == Phase::Released)
            (void)button->on_ui_input_event(pointer(ui::UiInputEventType::PointerReleased));
        else if (phase == Phase::Canceled)
            host.cancel_input_interaction();
        require(called && host.child_count() == 0,"callbacks must safely remove their own subtree");
    }
    ui::UiChildHost host;
    int canceled = 0;
    for (int index = 0; index < 3; ++index)
    {
        auto owned = std::make_unique<ui::UiActionButton>(core::Rect{ 0,0,40,40 });
        auto* button = owned.get();
        button->set_on_interaction([&,index](const auto& event) {
            if (event.phase == Phase::Canceled)
            {
                ++canceled;
                if (index != 2) throw std::runtime_error(index == 0 ? "first" : "second");
            }
        });
        host.add_child(std::move(owned));
        press(*button);
    }
    bool failed = false;
    try { host.cancel_input_interaction(); }
    catch (const std::runtime_error& e) { failed = std::string_view(e.what()) == "first"; }
    require(failed && canceled == 3,"batch cancellation must finish all children and preserve first failure");
    host.cancel_input_interaction();
    require(canceled == 3,"throwing cancellations must have cleared pressed state");

    ui::UiActionButton button(core::Rect{ 0,0,40,40 });
    button.set_on_interaction([](const auto&) { throw std::runtime_error("press"); });
    try { press(button); require(false,"press exception must propagate"); }
    catch (const std::runtime_error&) {}
    require(button.is_pointer_pressed(),"press state must update before callback");
    try { (void)button.on_ui_input_event(pointer(ui::UiInputEventType::PointerReleased)); require(false,"release exception must propagate"); }
    catch (const std::runtime_error&) {}
    require(!button.is_pointer_pressed(),"release state must update before callback");
}

class TestScene final : public scene::Scene
{
public:
    void on_enter(const scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
    void on_routed_input(const input::InputSnapshot& snapshot) override { routed = snapshot; }
    void clear() { clear_scene_objects(); }
    input::InputSnapshot routed;
};

void test_scene_routing_and_focus_loss()
{
    TestScene scene;
    auto* first = scene.create_and_add_object<ui::UiActionButton>(core::Rect{ 0,0,40,40 });
    auto* second = scene.create_and_add_object<ui::UiActionButton>(core::Rect{ 50,0,40,40 });
    scene.set_ui_gamepad(input::InputSourceId::gamepad(7));
    tests::InputSnapshotBuilder devices;
    scene::SceneTestAccess::route_input(scene,devices.take());
    devices.press(input::RawInputControl::KeyE,true);
    devices.press(input::RawInputControl::GamepadSouth,true);
    devices.event({ .control=input::RawInputControl::MouseLeft,.type=input::RawInputEventType::ControlPressed,
        .device=input::InputDevice::Mouse,.mouse_x=10,.mouse_y=10,.source=input::InputSourceId::mouse() });
    scene::SceneTestAccess::route_input(scene,devices.take());
    require(first->is_pointer_pressed() && !second->is_pointer_pressed(),"scene must route pointer input to matching HUD");
    require(scene.routed.events.size() == 2 && scene.routed.events[0].control == input::RawInputControl::KeyE
        && scene.routed.events[1].control == input::RawInputControl::GamepadSouth,
        "HUD click must consume its pointer event while gameplay keyboard and gamepad pass through");
    devices.event({ .control=input::RawInputControl::MouseLeft,.type=input::RawInputEventType::ControlReleased,
        .device=input::InputDevice::Mouse,.mouse_x=10,.mouse_y=10,.source=input::InputSourceId::mouse() });
    scene::SceneTestAccess::route_input(scene,devices.take());
    require(scene.routed.events.empty(),"HUD-owned release must not leak to gameplay");
    press(*first);
    (void)second->on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed,60,10));
    int canceled = 0;
    first->set_on_interaction([&](const auto& event) { if (event.phase == Phase::Canceled) { ++canceled; throw std::runtime_error("root cancellation"); } });
    second->set_on_interaction([&](const auto& event) { if (event.phase == Phase::Canceled) ++canceled; });
    auto lost = devices.take();
    lost.focus_lost = true;
    bool failed = false;
    try { scene::SceneTestAccess::route_input(scene,lost); }
    catch (const std::runtime_error&) { failed = true; }
    require(failed && canceled == 2 && !first->is_pressed() && !second->is_pressed(),
        "focus loss must cancel all roots before propagating callback failure");

    press(*first);
    first->set_on_interaction([&](const auto& event) { if (event.phase == Phase::Canceled) scene.clear(); });
    scene::SceneTestAccess::route_input(scene,lost);
    // Both old root lifetime snapshots have expired. New roots wait for the next batch.
    auto* replacement = scene.create_and_add_object<ui::UiActionButton>(core::Rect{ 0,0,40,40 });
    require(replacement != nullptr,"scene must remain usable after cancellation removes all roots");

    ui::UiPanel panel(core::Rect{ 0,0,100,100 });
    panel.add_child(std::make_unique<ui::UiActionButton>(core::Rect{ 0,0,40,40 }));
    panel.set_scope_focused(true);
    require(!panel.focus_first_available(),"HUD slot must not enter panel focus graph");
    require(panel.on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed)),"nonfocusable HUD must receive panel pointer input");
}

void test_theme_and_geometry()
{
    ui::UiChildHost root(core::Rect{ 0,0,200,200 });
    auto owned_host = std::make_unique<ui::UiChildHost>(core::Rect{ 0,0,100,100 });
    auto& host = *owned_host;
    root.add_child(std::move(owned_host));
    auto owned = std::make_unique<ui::UiActionButton>(core::Rect{ 5,5,40,40 });
    auto* button = owned.get();
    host.add_child(std::move(owned));
    ui::UiThemeManager manager;
    auto registration = manager.register_root(host);
    ui::UiActionButtonStyleOverrides overrides;
    overrides.overlay = core::Color{ 10,20,30,100 };
    button->set_style_overrides(overrides);
    manager.set_theme(ui::UiBuiltinTheme::ElysiaLight);
    require(button->style().chrome.background.idle == manager.current_theme().button().chrome.background.idle
        && button->style().overlay == *overrides.overlay,"theme must update base colors without losing local overrides");
    button->clear_style_overrides();
    require(!button->has_style_overrides(),"overrides must be clearable");
    host.set_presentation_translation({ 50,20 });
    button->set_presentation_translation({ 10,5 });
    require(!button->on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed,10,10)),"layout coordinates must not hit translated HUD");
    require(button->on_ui_input_event(pointer(ui::UiInputEventType::PointerPressed,70,35)),"accumulated presentation translation must affect hit tests");
    button->cancel_input_interaction();
    host.set_opacity(128);
    host.set_clip_children(true);
    button->set_opacity(128);
    std::vector<core::UiRenderCommand> commands;
    root.submit_ui_render_commands(commands);
    require(!commands.empty() && commands[0].screen_rect.x() == 65 && commands[0].screen_rect.y() == 30
        && commands[0].use_clip_rect,"host must apply presentation translation and clipping to HUD commands");
    const auto alpha = button->style().chrome.background.idle.a;
    require(commands[0].color.a == (alpha * 128 / 255) * 128 / 255,"host and widget opacity must compose");
}

void test_render_content_and_order()
{
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),"HUD rendering test must initialize SDL");
    require(TTF_Init(),"HUD rendering test must initialize SDL_ttf");
    require(tests::open_test_mixer(),"built-in HUD resources require a test mixer");
    auto* surface = SDL_CreateSurface(256,128,SDL_PIXELFORMAT_RGBA32);
    auto* renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr,"HUD rendering test must create software renderer");
    auto* paths = io::PathManager::instance();
    auto* resources = resources::ResourceManager::instance();
    auto* localization = localization::LocalizationManager::instance();
    require(paths->initialize(),"project paths must initialize");
    const auto settings = typography::resolve_font_settings(typography::FontSettings{});
    require(settings.has_value(),"font settings must resolve");
    auto& builtin = *builtin::BuiltinResources::instance();
    require(builtin.initialize(renderer,builtin::BuiltinAssetCatalog(*paths),settings->engine_point_sizes(),{}).has_value(),
        "HUD rendering test must load built-in fonts");
    typography::FontResolver fonts;
    require(localization->initialize(renderer,paths->configs() / "manifests" / "i18n_manifest.json","en",&fonts),
        "HUD localization must initialize");
    require(fonts.configure(*settings,*resources::ResourceService::instance(),ELYSIA_LOCALIZATION->supported_languages()).has_value(),
        "HUD fonts must configure");
    auto* icon = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,32,32);
    require(icon != nullptr,"icon texture must exist");
    {
        ui::UiActionButton button(core::Rect{ 0,0,100,80 });
        auto style = button.style();
        style.chrome.corner_radius = 0;
        style.chrome.background.idle = { 1,2,3,200 };
        style.overlay = { 4,5,6,160 };
        style.selected_border = { 7,8,9,255 };
        button.set_base_style(style);
        button.set_opacity(128);
        button.set_content(ui::UiActionButtonIconContent{ icon,core::Rect{ 8,4,16,8 } });
        button.set_key_hint(ui::ui_raw_text("E"));
        button.set_badge_text(ui::ui_raw_text("12"));
        button.set_selected(true);
        button.set_overlay_ratio(0.5f);
        std::vector<core::UiRenderCommand> commands;
        button.submit_ui_render_commands(commands);
        require(commands.size() == 7,"HUD must draw background, icon, overlay, both borders and two labels");
        require(commands[0].type == core::UiRenderCommandType::FillRect && commands[0].color.a == 100,
            "background must be first and apply opacity");
        require(commands[1].texture == icon && commands[1].use_src_rect
            && commands[1].src_rect.nearly_equals({ 8,4,16,8 })
            && commands[1].screen_rect.width() == 92 && commands[1].screen_rect.height() == 46,
            "cropped icon must fit using source aspect ratio");
        require(commands[2].screen_rect.nearly_equals({ 0,40,100,40 }) && commands[2].color.a == 80,
            "overlay must cover bottom fraction above content");
        require(commands[3].type == core::UiRenderCommandType::DrawRect
            && commands[4].color.r == 7 && commands[5].texture && commands[6].texture,
            "borders must precede independent text annotations");
        require(commands[5].screen_rect.left() == 4 && commands[5].screen_rect.bottom() == 76
            && commands[6].screen_rect.right() == 96 && commands[6].screen_rect.top() == 4,
            "key hint must anchor bottom-left and badge top-right");
        button.set_overlay_ratio(1);
        commands.clear(); button.submit_ui_render_commands(commands);
        require(commands[2].screen_rect.nearly_equals(button.screen_rect()),"full overlay must cover complete slot");
        button.set_overlay_ratio(0);
        commands.clear(); button.submit_ui_render_commands(commands);
        require(commands.size() == 6,"zero overlay must emit no mask command");
        button.set_content(ui::ui_text_key("common.confirm"));
        commands.clear(); button.submit_ui_render_commands(commands);
        require(commands[1].texture && commands[1].texture != icon,"localized text content must use shared text rendering");
        button.set_enabled(false);
        commands.clear(); button.submit_ui_render_commands(commands);
        require(commands[0].color.r == style.chrome.background.disabled.r,"disabled appearance must use disabled theme color");
        button.set_content(std::monostate{});
        button.set_key_hint({}); button.set_badge_text({});
        commands.clear(); button.submit_ui_render_commands(commands);
        require(commands.size() == 3,"empty content must retain background and selection borders");
    }
    SDL_DestroyTexture(icon);
    localization->shutdown(); fonts.shutdown(); resources->clear(); builtin.shutdown();
    SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface); tests::close_test_mixer(); TTF_Quit(); SDL_Quit();
}
}

int main()
{
    test_interaction_and_external_state();
    test_callback_lifetime_and_batch_cancellation();
    test_scene_routing_and_focus_loss();
    test_theme_and_geometry();
    test_render_content_and_order();
    return EXIT_SUCCESS;
}
