#include "game/gameplay/control/local_controls.h"
#include "game/input/gameplay_actions.h"
#include "game/showcase/effects/effects_showcase_scene.h"

#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/object/engine_character.h"
#include "engine/core/render/colors.h"
#include "engine/effects/effect_service.h"
#include "engine/input/raw_input_types.h"
#include "engine/ui/widgets/image/ui_animation.h"
#include "engine/ui/window/ui_window.h"
#include "engine/scene/runtime/scene_runtime_context.h"

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


}

EffectsShowcaseScene::~EffectsShowcaseScene()
{
    cancel_screen_effect();
    restore_character_debug_draw();
}

EffectsShowcaseScene::EffectsShowcaseScene()
    : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
          .camera = elysia::scene::CameraSceneConfig{}})
{}

void EffectsShowcaseScene::on_after_update(double delta)
{
    (void)delta;
    refresh_character_debug_draw();
    refresh_screen_status();
}

void EffectsShowcaseScene::on_shortcuts(const elysia::input::RawInputFrame &input,
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

void EffectsShowcaseScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    // This lab is an always-open interactive control panel, rather than a passive HUD.
    set_ui_interaction_mode(elysia::scene::UiInteractionMode::Navigation);
    const example::scene::ShowcaseEnterPayload* test_payload =
        elysia::scene::try_scene_payload<
            example::scene::ShowcaseEnterPayload>(payload);
    if (!test_payload || !is_valid_return_route(test_payload->return_route))
        throw std::logic_error("EffectsShowcaseScene requires ShowcaseEnterPayload with a valid return route.");

    cancel_screen_effect();
    _screen_layer = elysia::effects::ScreenEffectLayer::AfterUi;
    _screen_action_key = "showcase.effects.screen.ready";
    try
    {
        _return_route = test_payload->return_route;
        _paused = false;
        if (!elysia::builtin::BuiltinResources::instance()->is_initialized())
            throw std::logic_error("EffectsShowcaseScene requires initialized BuiltinResources.");
        if (!_character || _character->is_destroyed())
        {
            _character = create_and_add_object<elysia::builtin::EngineCharacter>(example::input::actions::Move);
            if (!_character)
            {
                throw std::runtime_error(
                    "EffectsShowcaseScene could not create EngineCharacter.");
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
                elysia::core::Rect{ 160.0f,252.0f,292.0f,292.0f });
            if (!_primary_animation->set_engine_animation(
                    elysia::builtin::BuiltinAnimationId::EngineCharacterMove))
            {
                throw std::logic_error(
                    "EffectsShowcaseScene could not bind the character move animation.");
            }
        }
        if (!_secondary_animation)
        {
            _secondary_animation = create_and_add_object<elysia::ui::UiAnimation>(
                elysia::core::Rect{ 760.0f,256.0f,324.0f,284.0f });
            if (!_secondary_animation->set_engine_animation(
                    elysia::builtin::BuiltinAnimationId::EngineCharacterMove))
            {
                throw std::logic_error(
                    "EffectsShowcaseScene could not bind the character move animation.");
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
        refresh_screen_status();

        enable_character_debug_draw();
        refresh_character_debug_draw();
        if (_character)
            example::gameplay::configure_scene_player(*this, _controller, *_character);
    }
    catch (...)
    {
        restore_character_debug_draw();
        destroy_feature_controls();
        throw;
    }
}

void EffectsShowcaseScene::on_exit()
{
    destroy_feature_controls();
    _paused = false;
    if (_character)
        _character->clear_movement_input();
    if (_primary_animation)
        _primary_animation->pause();
    if (_secondary_animation)
        _secondary_animation->pause();
    elysia::tools::DebugDraw::instance()->clear_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider);
    restore_character_debug_draw();
}

void EffectsShowcaseScene::on_reset()
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

std::size_t EffectsShowcaseScene::color_overlay_index() const noexcept
{
    return _color_overlay_index;
}

void EffectsShowcaseScene::apply_secondary_color_overlay()
{
    if (_secondary_animation)
    {
        _secondary_animation->set_color_overlay(
            kColorOverlays[_color_overlay_index]);
    }
}

void EffectsShowcaseScene::build_feature_controls()
{
    _controls_window=create_and_add_object<elysia::ui::UiWindow>(elysia::core::Rect{0,0,float(runtime_context().logical_width()),float(runtime_context().logical_height())},100);
    _controls_window->set_style_overrides({.draw_background=false,.draw_border=false});
    _view.build(*_controls_window,{[this]{spawn_floating_number_effect(FloatingNumberPreset::Damage);},[this]{spawn_floating_number_effect(FloatingNumberPreset::Critical);},
        [this]{spawn_floating_number_effect(FloatingNumberPreset::Heal);},[this]{spawn_floating_number_effect(FloatingNumberPreset::Percent);},
        [this]{spawn_floating_number_effect(FloatingNumberPreset::Fraction);},[this]{spawn_floating_number_effect(FloatingNumberPreset::Decimal);}},
        {[this]{trigger_screen_action(ScreenAction::Flash);},[this]{trigger_screen_action(ScreenAction::FadeBlack);},
         [this]{trigger_screen_action(ScreenAction::HoldBlack);},[this]{trigger_screen_action(ScreenAction::Stop);},
         [this]{trigger_screen_action(ScreenAction::Cancel);},[this]{trigger_screen_action(ScreenAction::Stretch);},
         [this]{trigger_screen_action(ScreenAction::Cover);},[this]{trigger_screen_action(ScreenAction::Contain);},
         [this]{trigger_screen_action(ScreenAction::ToggleLayer);}},[this]{clear_screen_effect_or_return();});
}

void EffectsShowcaseScene::destroy_feature_controls() noexcept
{
    cancel_screen_effect();
    _view.clear();
    _screen_layer = elysia::effects::ScreenEffectLayer::AfterUi;
    _screen_action_key = "showcase.effects.screen.ready";
    if (_controls_window)
        _controls_window->destroy();
    _controls_window = nullptr;
}

void EffectsShowcaseScene::trigger_screen_action(ScreenAction action)
{
    using namespace elysia::effects;
    auto* service = ELYSIA_EFFECTS;
    if (action == ScreenAction::Stop)
    {
        _screen_action_key = _screen_effect && service->stop_screen_effect(*_screen_effect) ? "showcase.effects.screen.stopping" : "showcase.effects.screen.nothing_to_stop";
        refresh_screen_status();
        return;
    }
    cancel_screen_effect();
    if (action == ScreenAction::Cancel) _screen_action_key = "showcase.effects.screen.cancelled";
    else if (action == ScreenAction::ToggleLayer)
    {
        _screen_layer = _screen_layer == ScreenEffectLayer::AfterUi ? ScreenEffectLayer::BeforeUi : ScreenEffectLayer::AfterUi;
        _screen_action_key = "showcase.effects.screen.layer_changed";
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
        _screen_action_key = action == ScreenAction::Stretch ? "showcase.effects.screen.stretch" : action == ScreenAction::Cover ? "showcase.effects.screen.cover" : "showcase.effects.screen.contain";
        if (!_screen_effect) _screen_action_key = "showcase.effects.screen.image_unavailable";
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
            _screen_action_key = "showcase.effects.screen.flash";
        }
        else
        {
            request.color = {0,0,0};
            request.playback.fade_in_seconds = 0.4;
            request.playback.fade_out_seconds = 0.4;
            request.playback.hold_seconds = 0.3;
            request.playback.end = action == ScreenAction::HoldBlack ? ScreenEffectEnd::Manual : ScreenEffectEnd::Timed;
            _screen_action_key = action == ScreenAction::HoldBlack ? "showcase.effects.screen.hold_black" : "showcase.effects.screen.fade_black";
        }
        _screen_effect = service->request_screen_color_effect(request);
        if (!_screen_effect) _screen_action_key = "showcase.effects.screen.failed";
    }
    refresh_screen_status();
}

void EffectsShowcaseScene::cancel_screen_effect() noexcept
{
    if (_screen_effect) (void)ELYSIA_EFFECTS->cancel_screen_effect(*_screen_effect);
    _screen_effect.reset();
}

void EffectsShowcaseScene::refresh_screen_status()
{
    _view.update_screen({_screen_action_key,
        _screen_effect && ELYSIA_EFFECTS->is_screen_effect_active(*_screen_effect),
        _screen_effect.has_value(), _screen_layer == elysia::effects::ScreenEffectLayer::AfterUi});
}

void EffectsShowcaseScene::clear_screen_effect_or_return()
{
    if (_screen_effect && ELYSIA_EFFECTS->is_screen_effect_active(*_screen_effect))
        trigger_screen_action(ScreenAction::Cancel);
    else return_to_caller();
}

void EffectsShowcaseScene::spawn_floating_number_effect(
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

void EffectsShowcaseScene::enable_character_debug_draw()
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

void EffectsShowcaseScene::restore_character_debug_draw() noexcept
{
    if (!_debug_draw_state_captured)
        return;

    elysia::tools::DebugDraw* debug_draw =
        elysia::tools::DebugDraw::instance();
    debug_draw->set_enabled(_previous_debug_draw_enabled);
    debug_draw->set_enabled_categories(_previous_debug_draw_categories);
    _debug_draw_state_captured = false;
}

void EffectsShowcaseScene::refresh_character_debug_draw()
{
    elysia::tools::DebugDraw* debug_draw =
        elysia::tools::DebugDraw::instance();
    debug_draw->clear_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider);
    if (_character && !_character->is_destroyed())
        _character->submit_debug_draw();
}

void EffectsShowcaseScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}
}
