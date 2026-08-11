#include "engine_feature_test_scene.h"

#include "../../../engine/builtin/resources/builtin_asset_cache.h"
#include "../../../engine/builtin/resources/builtin_asset_keys.h"
#include "../../../engine/builtin/object/engine_character.h"
#include "../../../engine/core/render/colors.h"
#include "../../../engine/effects/effect_service.h"
#include "../../../engine/input/raw_input_types.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/widgets/image/ui_animation.h"
#include "../../../engine/ui/window/ui_window.h"
#include "../../../engine/scene/runtime/scene_runtime_context.h"

#include <stdexcept>
#include <array>
#include <optional>

namespace example::testbed
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

void EngineFeatureTestScene::on_update(double delta)
{
    elysia::scene::Scene::on_update(delta);
    refresh_character_debug_draw();
}

void EngineFeatureTestScene::on_input(
    const elysia::input::RawInputFrame& input,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    elysia::scene::Scene::on_input(input,events);
    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.control == elysia::input::RawInputControl::KeyEscape
            && event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            return_to_caller();
            return;
        }
        if (event.control == elysia::input::RawInputControl::KeySpace
            && event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            _color_overlay_index =
                (_color_overlay_index + 1) % kColorOverlays.size();
            apply_secondary_color_overlay();
        }
    }
}

void EngineFeatureTestScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const TestbedScenePayload* test_payload =
        elysia::scene::try_scene_payload<TestbedScenePayload>(payload);
    if (!test_payload || !is_valid_return_route(test_payload->return_route))
        throw std::logic_error("EngineFeatureTestScene requires TestbedScenePayload with a valid return route.");

    _return_route = test_payload->return_route;
    _paused = false;
    const auto* cache = runtime_context().builtin_asset_cache();
    if (!cache || !cache->is_initialized())
        throw std::logic_error("EngineFeatureTestScene requires an initialized BuiltinAssetCache.");
    if (!_character || _character->is_destroyed())
    {
        _character = create_and_add_object<elysia::builtin::EngineCharacter>(
            *cache);
        if (!_character)
        {
            throw std::runtime_error(
                "EngineFeatureTestScene could not create EngineCharacter.");
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
                *cache,
                elysia::builtin::asset_keys::EngineCharacterMoveAnimation))
        {
            throw std::logic_error(
                "EngineFeatureTestScene could not bind the character move animation.");
        }
    }
    if (!_secondary_animation)
    {
        _secondary_animation = create_and_add_object<elysia::ui::UiAnimation>(
            elysia::core::Rect{ 760.0f,204.0f,324.0f,284.0f });
        if (!_secondary_animation->set_engine_animation(
                *cache,
                elysia::builtin::asset_keys::EngineCharacterMoveAnimation))
        {
            throw std::logic_error(
                "EngineFeatureTestScene could not bind the character move animation.");
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

    enable_character_debug_draw();
    refresh_character_debug_draw();
}

void EngineFeatureTestScene::on_exit()
{
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

void EngineFeatureTestScene::reset()
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

std::size_t EngineFeatureTestScene::color_overlay_index() const noexcept
{
    return _color_overlay_index;
}

void EngineFeatureTestScene::apply_secondary_color_overlay()
{
    if (_secondary_animation)
    {
        _secondary_animation->set_color_overlay(
            kColorOverlays[_color_overlay_index]);
    }
}

void EngineFeatureTestScene::build_feature_controls()
{
    _controls_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{ 390.0f,540.0f,500.0f,124.0f },100);
    if (!_controls_window)
    {
        throw std::runtime_error(
            "EngineFeatureTestScene could not create its feature control window.");
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

void EngineFeatureTestScene::destroy_feature_controls() noexcept
{
    if (_controls_window)
        _controls_window->destroy();
    _controls_window = nullptr;
}

void EngineFeatureTestScene::spawn_floating_number_effect(
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

void EngineFeatureTestScene::enable_character_debug_draw()
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

void EngineFeatureTestScene::restore_character_debug_draw() noexcept
{
    if (!_debug_draw_state_captured)
        return;

    elysia::tools::DebugDraw* debug_draw =
        elysia::tools::DebugDraw::instance();
    debug_draw->set_enabled(_previous_debug_draw_enabled);
    debug_draw->set_enabled_categories(_previous_debug_draw_categories);
    _debug_draw_state_captured = false;
}

void EngineFeatureTestScene::refresh_character_debug_draw()
{
    elysia::tools::DebugDraw* debug_draw =
        elysia::tools::DebugDraw::instance();
    debug_draw->clear_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider);
    if (_character && !_character->is_destroyed())
        _character->submit_debug_draw();
}

void EngineFeatureTestScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}
}
