#pragma once
#include "game/navigation/showcase_scene_keys.h"
#include "engine/scene/routing/scene_route.h"
#include <array>
namespace example::showcase
{
struct ShowcaseEntry
{
    const char* title;
    const char* description;
    elysia::scene::SceneKey key;
    elysia::scene::SceneReloadMode reload;
};
inline constexpr std::array<ShowcaseEntry,8> kShowcaseEntries{{
    {"showcase.ui.title","showcase.ui.description",scene_keys::UiComponentGallery,elysia::scene::SceneReloadMode::Reuse},
    {"showcase.input.title","showcase.input.description",scene_keys::LocalMultiplayer,elysia::scene::SceneReloadMode::Recreate},
    {"showcase.camera.title","showcase.camera.description",scene_keys::CameraShowcase,elysia::scene::SceneReloadMode::Reuse},
    {"showcase.animation.title","showcase.animation.description",scene_keys::AnimationPreview,elysia::scene::SceneReloadMode::Reuse},
    {"showcase.effects.title","showcase.effects.description",scene_keys::EffectsShowcase,elysia::scene::SceneReloadMode::Reuse},
    {"showcase.audio.title","showcase.audio.description",scene_keys::AudioShowcase,elysia::scene::SceneReloadMode::Recreate},
    {"showcase.physics.title","showcase.physics.description",scene_keys::PhysicsGallery,elysia::scene::SceneReloadMode::Reuse},
    {"showcase.gameplay.title","showcase.gameplay.description",scene_keys::GameplayGallery,elysia::scene::SceneReloadMode::Reuse}
}};
}
