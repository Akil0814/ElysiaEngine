#include "../../input/local_controls.h"
#include "engine_feature_lab_scene.h"

#include "../../../engine/builtin/resources/builtin_resources.h"
#include "../../../engine/builtin/object/engine_character.h"
#include "../../../engine/core/render/colors.h"
#include "../../../engine/effects/effect_service.h"
#include "../../../engine/input/raw_input_types.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/widgets/image/ui_animation.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/window/ui_window.h"
#include "../../../engine/scene/runtime/scene_runtime_context.h"

#include <stdexcept>
#include <array>
#include <optional>

namespace example::scene
{
namespace
{
bool is_valid_return_route(const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}

const std::array<std::optional<elysia::core::Color>,5> kColorOverlays = {
    std::nullopt,
    elysia::core::colors::white,
    elysia::core::colors::blue_500,
    elysia::core::colors::purple_500,
    elysia::core::colors::gray_700
};

constexpr float kControlButtonWidth = 144.0f;
constexpr float kControlButtonHeight = 52.0f;
constexpr float kControlSpacing = 16.0f;

std::unique_ptr<elysia::ui::UiButton> make_control_button(const char* label)
{
    auto button = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{ 0,0,kControlButtonWidth,kControlButtonHeight });
    button->set_text_content(elysia::ui::ui_raw_text(label));
    return button;
}
}

EngineFeatureLabScene::EngineFeatureLabScene()
    : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
          .camera = elysia::scene::CameraSceneConfig{}})
{}

void EngineFeatureLabScene::on_after_update(double delta)
{
    (void)delta;
    refresh_character_debug_draw();
    refresh_screen_status();
}

void EngineFeatureLabScene::on_shortcuts(const elysia::input::RawInputFrame &input,
                                         const std::vector<elysia::input::RawInputEvent> &events)
{

    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.control == elysia::input::RawInputControl::KeyEscape
            && event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            consume_input(event);
            clear_screen_effect_or_return();
            return;
        }
        if (event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            using Control = elysia::input::RawInputControl;
            const std::array<Control, 9> shortcuts{Control::KeyF1, Control::KeyF2, Control::KeyF3,
                Control::KeyF4, Control::KeyF5, Control::KeyF6, Control::KeyF7, Control::KeyF8, Control::KeyF9};
            for (std::size_t i = 0; i < shortcuts.size(); ++i)
                if (event.control == shortcuts[i])
                {
                    consume_input(event);
                    trigger_screen_action(static_cast<ScreenAction>(i));
                    return;
                }
        }
        if (event.control == elysia::input::RawInputControl::KeySpace
            && event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            consume_input(event);
            _color_overlay_index =
                (_color_overlay_index + 1) % kColorOverlays.size();
            apply_secondary_color_overlay();
        }
    }
}

void EngineFeatureLabScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    // This lab is an always-open interactive control panel, rather than a passive HUD.
    set_ui_interaction_mode(elysia::input::UiInteractionMode::Navigation);
    const example::scene::DemoScenePayload* test_payload =
        elysia::scene::try_scene_payload<
            example::scene::DemoScenePayload>(payload);
    if (!test_payload || !is_valid_return_route(test_payload->return_route))
        throw std::logic_error("EngineFeatureLabScene requires DemoScenePayload with a valid return route.");

    _return_route = test_payload->return_route;
    _paused = false;
    if (!elysia::builtin::BuiltinResources::instance()->is_initialized())
        throw std::logic_error("EngineFeatureLabScene requires initialized BuiltinResources.");
    if (!_character || _character->is_destroyed())
    {
        _character = create_and_add_object<elysia::builtin::EngineCharacter>(example::input::actions::Move);
        if (!_character)
        {
            throw std::runtime_error(
                "EngineFeatureLabScene could not create EngineCharacter.");
        }
        _character->set_center(elysia::core::Vector2::zero());
    }

    elysia::core::Rect movement_bounds = camera().view_rect();
    if (movement_bounds.is_empty())
    {
        movement_bounds = elysia::core::Rect::from_center(
            elysia::core::Vector2::zero(),
            elysia::core::Vector2{
                static_cast<float>(runtime_context().logical_width()),
                static_cast<float>(runtime_context().logical_height())});
    }
    _character->set_movement_bounds(movement_bounds);

    if (!_primary_animation)
    {
        _primary_animation = create_and_add_object<elysia::ui::UiAnimation>(
            elysia::core::Rect{ 160.0f,200.0f,292.0f,292.0f });
        if (!_primary_animation->set_engine_animation(
                elysia::builtin::BuiltinAnimationId::EngineCharacterMove))
        {
            throw std::logic_error(
                "EngineFeatureLabScene could not bind the character move animation.");
        }
    }
    if (!_secondary_animation)
    {
        _secondary_animation = create_and_add_object<elysia::ui::UiAnimation>(
            elysia::core::Rect{ 760.0f,204.0f,324.0f,284.0f });
        if (!_secondary_animation->set_engine_animation(
                elysia::builtin::BuiltinAnimationId::EngineCharacterMove))
        {
            throw std::logic_error(
                "EngineFeatureLabScene could not bind the character move animation.");
        }
    }
    _primary_animation->play();
    _secondary_animation->play();
    apply_secondary_color_overlay();

    if (!_controls_window || _controls_window->is_destroyed())
        build_feature_controls();
    _controls_window->set_visible(true);
    _controls_window->set_active(true);
    _controls_window->focus_first_available_scope();
    if (!_screen_controls_window || _screen_controls_window->is_destroyed())
        build_screen_controls();
    _screen_controls_window->set_visible(true);
    _screen_controls_window->set_active(true);
    _screen_effect.reset();
    _screen_action = "Ready";
    refresh_screen_status();

    try
    {
        enable_character_debug_draw();
        refresh_character_debug_draw();
        if (_character)
            example::input::configure_scene_player(*this, _controller, *_character);
    }
    catch (...)
    {
        restore_character_debug_draw();
        throw;
    }
}

void EngineFeatureLabScene::on_exit()
{
    _screen_effect.reset();
    if (_screen_controls_window && !_screen_controls_window->is_destroyed())
    {
        _screen_controls_window->set_active(false);
        _screen_controls_window->set_visible(false);
    }
    _paused = false;
    if (_character)
        _character->clear_movement_input();
    if (_primary_animation)
        _primary_animation->pause();
    if (_secondary_animation)
        _secondary_animation->pause();
    if (_controls_window && !_controls_window->is_destroyed())
    {
        _controls_window->set_active(false);
        _controls_window->set_visible(false);
    }
    elysia::tools::DebugDraw::instance()->clear_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider);
    restore_character_debug_draw();
}

void EngineFeatureLabScene::on_reset()
{
    _paused = false;
    _return_route = {};
    if (_primary_animation)
        _primary_animation->destroy();
    if (_secondary_animation)
        _secondary_animation->destroy();
    if (_character)
        _character->destroy();
    _primary_animation = nullptr;
    _secondary_animation = nullptr;
    _character = nullptr;
    destroy_feature_controls();
    _color_overlay_index = 2;
    elysia::tools::DebugDraw::instance()->clear_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider);
    restore_character_debug_draw();
}

std::size_t EngineFeatureLabScene::color_overlay_index() const noexcept
{
    return _color_overlay_index;
}

void EngineFeatureLabScene::apply_secondary_color_overlay()
{
    if (_secondary_animation)
    {
        _secondary_animation->set_color_overlay(
            kColorOverlays[_color_overlay_index]);
    }
}

void EngineFeatureLabScene::build_feature_controls()
{
    _controls_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{ 390.0f,540.0f,500.0f,124.0f },100);
    if (!_controls_window)
    {
        throw std::runtime_error(
            "EngineFeatureLabScene could not create its feature control window.");
    }

    auto number_scroll = std::make_unique<elysia::ui::UiScrollContainer>(
        elysia::core::Rect{ 18.0f,16.0f,464.0f,92.0f });
    number_scroll->set_scroll_axis(elysia::ui::UiScrollAxis::Horizontal);
    number_scroll->set_scrollbar_visibility(elysia::ui::UiScrollBarVisibility::Auto);
    number_scroll->set_scroll_step_x(kControlButtonWidth + kControlSpacing);

    constexpr std::size_t kPresetCount = 6;
    const float number_content_width =
        kPresetCount * kControlButtonWidth
        + (kPresetCount - 1) * kControlSpacing;
    auto number_controls = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0.0f,0.0f,number_content_width,kControlButtonHeight });
    number_controls->set_direction(elysia::ui::UiListDirection::Horizontal);
    number_controls->set_item_spacing(kControlSpacing);

    const auto add_number_button = [this,&number_controls](
        const char* label,
        FloatingNumberPreset preset)
    {
        auto button = make_control_button(label);
        button->set_on_click([this,preset]()
        {
            spawn_floating_number_effect(preset);
        });
        number_controls->add_back(std::move(button));
    };
    add_number_button("Damage",FloatingNumberPreset::Damage);
    add_number_button("Critical",FloatingNumberPreset::Critical);
    add_number_button("Heal",FloatingNumberPreset::Heal);
    add_number_button("Percent",FloatingNumberPreset::Percent);
    add_number_button("Fraction",FloatingNumberPreset::Fraction);
    add_number_button("Decimal",FloatingNumberPreset::Decimal);

    elysia::ui::UiScrollContainer* number_scroll_ptr = number_scroll.get();
    number_scroll->set_content(std::move(number_controls));
    _controls_window->add_child(std::move(number_scroll));
    _controls_window->register_focus_scope(*number_scroll_ptr);
}

void EngineFeatureLabScene::destroy_feature_controls() noexcept
{
    if (_screen_controls_window) _screen_controls_window->destroy();
    _screen_controls_window = nullptr;
    _screen_status = nullptr;
    _screen_layer_button = nullptr;
    _screen_effect.reset();
    _screen_layer = elysia::effects::ScreenEffectLayer::AfterUi;
    if (_controls_window)
        _controls_window->destroy();
    _controls_window = nullptr;
}

void EngineFeatureLabScene::build_screen_controls()
{
    using namespace elysia::ui;
    // Leave the existing bottom number panel and its keyboard focus unchanged.
    _screen_controls_window = create_and_add_object<UiWindow>(elysia::core::Rect{20,12,760,166},90);
    if (!_screen_controls_window) throw std::runtime_error("Could not create screen effect controls.");
    auto heading = std::make_unique<UiLabel>(elysia::core::Rect{0,0,728,22},0,
        ui_raw_text("Screen effects | F1-F9 or click | Esc clears, then returns"));
    _screen_controls_window->add_child(std::move(heading), {._margin = {16,8,0,0}});
    const std::array<const char*,9> labels{"F1 Flash", "F2 Fade black", "F3 Hold black", "F4 Stop", "F5 Cancel",
        "F6 Stretch", "F7 Cover", "F8 Contain", "F9 UI: After"};
    for (std::size_t start : {std::size_t{0},std::size_t{5}})
    {
        auto row = std::make_unique<UiListContainer>(elysia::core::Rect{0,0,728,40});
        row->set_direction(UiListDirection::Horizontal);
        row->set_item_spacing(6);
        const std::size_t end = start == 0 ? 5 : labels.size();
        for (std::size_t i = start; i < end; ++i)
        {
            auto button = std::make_unique<UiButton>(elysia::core::Rect{0,0,140,40});
            button->set_text_content(ui_raw_text(labels[i]));
            button->set_on_click([this,i] { trigger_screen_action(static_cast<ScreenAction>(i)); });
            if (i == 8) _screen_layer_button = button.get();
            row->add_back(std::move(button));
        }
        auto* scope = row.get();
        _screen_controls_window->add_child(std::move(row), {._margin = {16,start == 0 ? 34.0f : 78.0f,0,0}});
        _screen_controls_window->register_focus_scope(*scope);
    }
    auto status = std::make_unique<UiLabel>(elysia::core::Rect{0,0,728,26});
    _screen_status = status.get();
    _screen_controls_window->add_child(std::move(status), {._margin = {16,122,0,0}});
    _screen_controls_window->set_on_cancel([this] { clear_screen_effect_or_return(); });
}

void EngineFeatureLabScene::trigger_screen_action(ScreenAction action)
{
    using namespace elysia::effects;
    auto* service = ELYSIA_EFFECTS;
    if (action == ScreenAction::Stop)
    {
        _screen_action = _screen_effect && service->stop_screen_effect(*_screen_effect) ? "Stopping" : "Nothing to stop";
        refresh_screen_status();
        return;
    }
    if (_screen_effect) service->cancel_screen_effect(*_screen_effect);
    _screen_effect.reset();
    if (action == ScreenAction::Cancel) _screen_action = "Cancelled";
    else if (action == ScreenAction::ToggleLayer)
    {
        _screen_layer = _screen_layer == ScreenEffectLayer::AfterUi ? ScreenEffectLayer::BeforeUi : ScreenEffectLayer::AfterUi;
        _screen_action = "Layer changed; choose an effect";
    }
    else if (action == ScreenAction::Stretch || action == ScreenAction::Cover || action == ScreenAction::Contain)
    {
        ScreenImageEffectRequest request;
        request.texture_key = "demo.screen_effect";
        request.fit = action == ScreenAction::Stretch ? ScreenEffectFit::Stretch
            : action == ScreenAction::Cover ? ScreenEffectFit::Cover : ScreenEffectFit::Contain;
        request.playback.layer = _screen_layer;
        request.playback.fade_in_seconds = 0.3;
        request.playback.hold_seconds = 1;
        request.playback.fade_out_seconds = 0.5;
        _screen_effect = service->request_screen_image_effect(request);
        _screen_action = "Image: " + std::string(action == ScreenAction::Stretch ? "Stretch" : action == ScreenAction::Cover ? "Cover" : "Contain");
        if (!_screen_effect) _screen_action = "Image unavailable: demo.screen_effect";
    }
    else
    {
        ScreenColorEffectRequest request;
        request.playback.layer = _screen_layer;
        request.playback.hold_seconds = 0;
        if (action == ScreenAction::Flash)
        {
            request.color = {255,255,255};
            request.playback.fade_out_seconds = 0.25;
            _screen_action = "Flash";
        }
        else
        {
            request.color = {0,0,0};
            request.playback.fade_in_seconds = 0.4;
            request.playback.fade_out_seconds = 0.4;
            request.playback.hold_seconds = 0.3;
            request.playback.end = action == ScreenAction::HoldBlack ? ScreenEffectEnd::Manual : ScreenEffectEnd::Timed;
            _screen_action = action == ScreenAction::HoldBlack ? "Hold black (F4 fades, F5/Esc clears)" : "Fade black";
        }
        _screen_effect = service->request_screen_color_effect(request);
        if (!_screen_effect) _screen_action = "Effect request failed";
    }
    refresh_screen_status();
}

void EngineFeatureLabScene::refresh_screen_status()
{
    const bool active = _screen_effect && ELYSIA_EFFECTS->is_screen_effect_active(*_screen_effect);
    if (_screen_status && !_screen_status->is_destroyed())
        _screen_status->set_text_content(elysia::ui::ui_raw_text(
            std::string(active ? "Playing: " : _screen_effect ? "Finished: " : "") + _screen_action));
    if (_screen_layer_button && !_screen_layer_button->is_destroyed())
        _screen_layer_button->set_text_content(elysia::ui::ui_raw_text(
            _screen_layer == elysia::effects::ScreenEffectLayer::AfterUi ? "F9 UI: After" : "F9 UI: Before"));
}

void EngineFeatureLabScene::clear_screen_effect_or_return()
{
    if (_screen_effect && ELYSIA_EFFECTS->is_screen_effect_active(*_screen_effect))
        trigger_screen_action(ScreenAction::Cancel);
    else return_to_caller();
}

void EngineFeatureLabScene::spawn_floating_number_effect(
    FloatingNumberPreset preset)
{
    if (!_character || _character->is_destroyed())
        return;

    elysia::effects::FloatingNumberEffectSpawnRequest request;
    request.position = _character->world_rect().top_center();
    request.alignment = elysia::effects::FloatingNumberAlignment::Center;
    request.target_height = 28.0f;
    request.lifetime_seconds = 0.6;

    switch (preset)
    {
    case FloatingNumberPreset::Damage:
        request.text = "-128";
        request.color = elysia::effects::FloatingNumberColor::Red;
        request.effects.motion = elysia::effects::FloatingNumberLinearMotion{
            .offset = { 0.0f,-64.0f }
        };
        request.effects.scale = elysia::effects::FloatingNumberScale{
            .from_scale = 1.2f,
            .to_scale = 1.0f,
            .time_range = { 0.0f,0.25f }
        };
        request.effects.fade = elysia::effects::FloatingNumberFade{
            .time_range = { 0.6f,1.0f }
        };
        break;
    case FloatingNumberPreset::Critical:
        request.text = "-999";
        request.color = elysia::effects::FloatingNumberColor::Yellow;
        request.target_height = 32.0f;
        request.lifetime_seconds = 0.8;
        request.effects.motion = elysia::effects::FloatingNumberArcMotion{
            .offset = { 48.0f,-72.0f },
            .arc_height = 40.0f
        };
        request.effects.scale = elysia::effects::FloatingNumberScale{
            .from_scale = 1.8f,
            .to_scale = 1.0f,
            .time_range = { 0.0f,0.3f }
        };
        request.effects.fade = elysia::effects::FloatingNumberFade{
            .time_range = { 0.65f,1.0f }
        };
        break;
    case FloatingNumberPreset::Heal:
        request.text = "256";
        request.color = elysia::effects::FloatingNumberColor::Green;
        request.effects.motion = elysia::effects::FloatingNumberLinearMotion{
            .offset = { 0.0f,-56.0f }
        };
        request.effects.scale = elysia::effects::FloatingNumberScale{
            .from_scale = 0.75f,
            .to_scale = 1.15f,
            .time_range = { 0.0f,0.3f }
        };
        request.effects.fade = elysia::effects::FloatingNumberFade{
            .time_range = { 0.6f,1.0f }
        };
        break;
    case FloatingNumberPreset::Percent:
        request.text = "75%";
        request.color = elysia::effects::FloatingNumberColor::LightBlue;
        request.effects.motion = elysia::effects::FloatingNumberLinearMotion{
            .offset = { 0.0f,-40.0f }
        };
        request.effects.scale = elysia::effects::FloatingNumberScale{
            .from_scale = 1.0f,
            .to_scale = 1.25f,
            .time_range = { 0.1f,0.6f }
        };
        request.effects.fade = elysia::effects::FloatingNumberFade{
            .time_range = { 0.5f,1.0f }
        };
        break;
    case FloatingNumberPreset::Fraction:
        request.text = "3/10";
        request.color = elysia::effects::FloatingNumberColor::Orange;
        request.effects.motion = elysia::effects::FloatingNumberArcMotion{
            .offset = { -44.0f,-60.0f },
            .arc_height = 28.0f
        };
        request.effects.fade = elysia::effects::FloatingNumberFade{
            .time_range = { 0.55f,1.0f }
        };
        break;
    case FloatingNumberPreset::Decimal:
        request.text = "12.5";
        request.color = elysia::effects::FloatingNumberColor::Purple;
        request.effects.motion = elysia::effects::FloatingNumberLinearMotion{
            .offset = { 0.0f,-52.0f }
        };
        request.effects.scale = elysia::effects::FloatingNumberScale{
            .from_scale = 1.15f,
            .to_scale = 0.85f,
            .time_range = { 0.1f,0.7f }
        };
        request.effects.fade = elysia::effects::FloatingNumberFade{
            .time_range = { 0.6f,1.0f }
        };
        break;
    }

    (void)ELYSIA_EFFECTS->request_floating_number_effect(request);
}

void EngineFeatureLabScene::enable_character_debug_draw()
{
    elysia::tools::DebugDraw* debug_draw =
        elysia::tools::DebugDraw::instance();
    if (!_debug_draw_state_captured)
    {
        _previous_debug_draw_enabled = debug_draw->enabled();
        _previous_debug_draw_categories = debug_draw->enabled_categories();
        _debug_draw_state_captured = true;
    }

    debug_draw->set_enabled(true);
    debug_draw->set_enabled_categories(
        debug_draw->enabled_categories()
        | elysia::tools::DebugDrawCategory::PhysicsCollider);
}

void EngineFeatureLabScene::restore_character_debug_draw() noexcept
{
    if (!_debug_draw_state_captured)
        return;

    elysia::tools::DebugDraw* debug_draw =
        elysia::tools::DebugDraw::instance();
    debug_draw->set_enabled(_previous_debug_draw_enabled);
    debug_draw->set_enabled_categories(_previous_debug_draw_categories);
    _debug_draw_state_captured = false;
}

void EngineFeatureLabScene::refresh_character_debug_draw()
{
    elysia::tools::DebugDraw* debug_draw =
        elysia::tools::DebugDraw::instance();
    debug_draw->clear_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider);
    if (_character && !_character->is_destroyed())
        _character->submit_debug_draw();
}

void EngineFeatureLabScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}
}
