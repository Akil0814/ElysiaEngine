#define SDL_MAIN_HANDLED

#include "engine/builtin/object/engine_character.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/tools/debug_draw.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <vector>

namespace
{
using elysia::tests::require;

class SdlFixture
{
public:
    SdlFixture()
    {
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0,
            "EngineCharacter tests must initialize SDL video and audio");
        require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
            "EngineCharacter tests must initialize PNG support");
        require(TTF_Init() == 0,
            "EngineCharacter tests must initialize SDL_ttf");
        require(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0,
            "EngineCharacter tests must open SDL_mixer audio");
        _surface = SDL_CreateRGBSurfaceWithFormat(
            0, 256, 256, 32, SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr,
            "EngineCharacter tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr,
            "EngineCharacter tests must create a software renderer");
    }

    ~SdlFixture()
    {
        SDL_DestroyRenderer(_renderer);
        SDL_FreeSurface(_surface);
        Mix_CloseAudio();
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    }

    [[nodiscard]] SDL_Renderer* renderer() const noexcept { return _renderer; }

private:
    SDL_Surface* _surface = nullptr;
    SDL_Renderer* _renderer = nullptr;
};

void send_control(
    elysia::builtin::EngineCharacter& character,
    elysia::input::RawInputControl control,
    bool pressed)
{
    require(character.on_raw_input_event(elysia::input::RawInputEvent{
            .control = control,
            .type = pressed
                ? elysia::input::RawInputEventType::ControlPressed
                : elysia::input::RawInputEventType::ControlReleased,
            .device = elysia::input::InputDevice::Keyboard
        }),
        "EngineCharacter must consume movement key transitions");
}

elysia::core::RenderCommand render_character(
    const elysia::builtin::EngineCharacter& character)
{
    std::vector<elysia::core::RenderCommand> commands;
    character.submit_render_commands(commands);
    require(commands.size() == 1,
        "EngineCharacter must submit exactly one sprite render command");
    return commands.front();
}

void test_animation_switching_and_facing(
    elysia::builtin::BuiltinResources& resources)
{
    elysia::builtin::EngineCharacter character(resources);
    const auto idle_command = render_character(character);
    const auto* idle_definition = resources.find_animation(
        elysia::builtin::BuiltinAnimationId::EngineCharacterIdle);
    const auto* move_definition = resources.find_animation(
        elysia::builtin::BuiltinAnimationId::EngineCharacterMove);
    require(idle_definition && move_definition
            && idle_command.texture == idle_definition->atlas->frame_at(0)->_texture,
        "EngineCharacter must render the idle animation by default");
    require(!character.set_animations(
                resources,
                elysia::builtin::BuiltinAnimationId::EngineCharacterIdle,
                static_cast<elysia::builtin::BuiltinAnimationId>(255))
            && render_character(character).texture == idle_command.texture,
        "a partially invalid animation replacement must preserve the current animation set");

    send_control(character, elysia::input::RawInputControl::KeyD, true);
    character.update(0.0);
    const auto right_command = render_character(character);
    require(right_command.texture == move_definition->atlas->frame_at(0)->_texture
            && right_command.flip == elysia::core::SpriteFlip::Horizontal,
        "right movement must select move animation and horizontally flip the native left-facing sprite");

    send_control(character, elysia::input::RawInputControl::KeyD, false);
    send_control(character, elysia::input::RawInputControl::KeyW, true);
    character.update(0.0);
    require(render_character(character).flip == elysia::core::SpriteFlip::Horizontal,
        "vertical-only movement must preserve the last horizontal facing");

    send_control(character, elysia::input::RawInputControl::KeyW, false);
    character.update(0.0);
    require(render_character(character).texture
            == idle_definition->atlas->frame_at(0)->_texture,
        "releasing every movement key must restore the idle animation");

    send_control(character, elysia::input::RawInputControl::KeyA, true);
    character.update(0.0);
    require(render_character(character).flip == elysia::core::SpriteFlip::None,
        "left movement must use the native sprite orientation");
}

void test_movement_normalization_and_bounds(
    elysia::builtin::BuiltinResources& resources)
{
    elysia::builtin::EngineCharacter character(resources);
    character.set_position(elysia::core::Vector2::zero());
    send_control(character, elysia::input::RawInputControl::KeyD, true);
    character.update(1.0);
    const float single_axis_distance = character.position().length();
    require(std::fabs(single_axis_distance
            - elysia::builtin::EngineCharacter::kMovementSpeed) < 0.001f,
        "single-axis movement must use the configured units-per-second speed");

    character.clear_movement_input();
    character.set_position(elysia::core::Vector2::zero());
    send_control(character, elysia::input::RawInputControl::KeyD, true);
    send_control(character, elysia::input::RawInputControl::KeyS, true);
    character.update(1.0);
    require(std::fabs(character.position().length() - single_axis_distance) < 0.001f,
        "diagonal movement must be normalized to the single-axis speed");

    character.clear_movement_input();
    character.set_position(elysia::core::Vector2::zero());
    send_control(character, elysia::input::RawInputControl::KeyA, true);
    send_control(character, elysia::input::RawInputControl::KeyD, true);
    character.update(1.0);
    require(character.position() == elysia::core::Vector2::zero(),
        "opposing movement inputs must cancel");

    character.clear_movement_input();
    character.set_position(elysia::core::Vector2::zero());
    character.set_movement_bounds(elysia::core::Rect{-50.0f, -50.0f, 200.0f, 200.0f});
    send_control(character, elysia::input::RawInputControl::KeyD, true);
    send_control(character, elysia::input::RawInputControl::KeyS, true);
    character.update(10.0);
    require(character.position() == elysia::core::Vector2{54.0f, 54.0f},
        "movement bounds must keep the complete 96 by 96 character rectangle inside the viewport");

    const elysia::core::Vector2 clamped_position = character.position();
    character.update(-1.0);
    character.update(std::numeric_limits<double>::quiet_NaN());
    require(character.position() == clamped_position,
        "negative and non-finite deltas must not move EngineCharacter");
}

void test_collider_and_debug_draw(
    elysia::builtin::BuiltinResources& resources)
{
    elysia::builtin::EngineCharacter character(resources);
    const auto colliders = character.colliders();
    require(colliders.size() == 1,
        "EngineCharacter must expose one debug collider");
    const auto* aabb = std::get_if<elysia::physics::AabbShape>(
        &colliders.front().shape);
    require(aabb && aabb->local_rect == elysia::core::Rect{24.0f, 16.0f, 48.0f, 72.0f}
            && colliders.front().filter.category == 0
            && colliders.front().filter.mask == 0
            && colliders.front().response == elysia::physics::CollisionResponse::Ignore,
        "EngineCharacter collider must remain debug-only and use the documented local AABB");

    elysia::tools::DebugDraw* debug_draw = elysia::tools::DebugDraw::instance();
    debug_draw->clear();
    debug_draw->set_enabled(true);
    debug_draw->set_enabled_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider);
    character.set_position(elysia::core::Vector2{10.0f, 20.0f});
    character.submit_debug_draw();
    require(debug_draw->commands().size() == 1,
        "EngineCharacter must submit one debug collider command");
    const auto* first_rect = std::get_if<elysia::tools::DebugDrawRect>(
        &debug_draw->commands().front().primitive);
    require(first_rect
            && first_rect->rect == elysia::core::Rect{34.0f, 36.0f, 48.0f, 72.0f},
        "debug collider must translate local geometry by the character world position");

    debug_draw->clear_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider);
    character.set_position(elysia::core::Vector2{20.0f, 30.0f});
    character.submit_debug_draw();
    const auto* refreshed_rect = std::get_if<elysia::tools::DebugDrawRect>(
        &debug_draw->commands().front().primitive);
    require(debug_draw->commands().size() == 1 && refreshed_rect
            && refreshed_rect->rect == elysia::core::Rect{44.0f, 46.0f, 48.0f, 72.0f},
        "refreshing the collider snapshot must leave only the current world position");
    debug_draw->clear();
    debug_draw->set_enabled(false);
    debug_draw->set_enabled_categories(elysia::tools::DebugDrawCategory::All);
}
}

int main()
{
    SdlFixture fixture;
    elysia::builtin::BuiltinResources resources;
    require(resources.initialize(
                fixture.renderer(),
                elysia::builtin::BuiltinAssetCatalog(
                    std::filesystem::path{ELYSIA_SOURCE_DIR}),
                std::array{10, 20, 30, 40, 50, 60, 70},
                {})
                .has_value(),
        "EngineCharacter tests must initialize built-in resources");

    test_animation_switching_and_facing(resources);
    test_movement_normalization_and_bounds(resources);
    test_collider_and_debug_draw(resources);

    resources.shutdown();
    return EXIT_SUCCESS;
}
