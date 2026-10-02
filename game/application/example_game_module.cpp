#include "game/showcase/audio/audio_showcase_scene.h"
#include "game/showcase/gameplay/gameplay_gallery_scene.h"
#include "game/showcase/scenarios/scenario_scene.h"
#include "game/application/example_game_module.h"
#include "game/showcase/input/local_multiplayer_scene.h"
#include "game/showcase/camera/multi_target_camera_scene.h"

#include "game/navigation/main_menu_scene.h"
#include "game/showcase/animation/animation_preview_scene.h"
#include "game/showcase/shared/showcase_gallery_scene.h"
#include "game/showcase/effects/effects_showcase_scene.h"
#include "game/showcase/ui/ui_component_gallery_scene.h"
#include "game/showcase/gameplay/collider_combat_demo_scene.h"
#include "game/showcase/physics/box2d_lab_scene.h"
#include "game/showcase/physics/physics_gallery_scene.h"
#include "game/showcase/gameplay/platform_tile_combat_demo_scene.h"
#include "game/showcase/gameplay/top_down_tile_combat_demo_scene.h"
#include "game/navigation/showcase_scene_keys.h"

#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/startup_loading_scene.h"
#include "engine/scene/scene_manager.h"
#if ELYSIA_ENABLE_IMGUI
#include "engine/tools/imgui/imgui_development_overlay.h"
#endif

namespace example::application
{
elysia::application::ApplicationDescriptor GameModule::descriptor() const
{
    using elysia::scene::SceneReloadMode;
    using elysia::scene::SceneRoute;
    using elysia::builtin::StartupLoadingScenePayload;

    elysia::application::ApplicationDescriptor descriptor;
    descriptor.logical_width = 1280;
    descriptor.logical_height = 720;
    descriptor.presentation.render.texture_filter =
        elysia::application::ApplicationTextureFilter::Nearest;
    descriptor.presentation.ui.default_theme =
        elysia::ui::UiBuiltinTheme::QuietSlate;
    descriptor.presentation.startup.engine_logo =
        elysia::application::ApplicationEngineLogoVariant::White;
    descriptor.presentation.fonts.ui.source =
        elysia::typography::FontSource::Project;
    descriptor.presentation.fonts.floating_number.source =
        elysia::typography::FontSource::Project;
    descriptor.initial_route = SceneRoute{
        .target = elysia::builtin::SceneKeys::StartupLoading,
        .payload = StartupLoadingScenePayload{
            .success_route = SceneRoute{
                .target = example::scene_keys::MainMenu,
                .payload = elysia::scene::ScenePayload{},
                .reload_mode = SceneReloadMode::Reuse
            },
            .failure_route = std::nullopt,
            .project_logo = std::nullopt,
            .wait_for_logo_sequence = false,
            .wait_for_confirmation = true
        },
        .reload_mode = SceneReloadMode::Reuse
    };
    return descriptor;
}

void GameModule::register_scenes(
    elysia::scene::SceneManager& scene_manager) const
{
    scene_manager.register_game_scene<example::scene::AudioShowcaseScene>(example::scene_keys::AudioShowcase);
    scene_manager.register_game_scene<example::scene::GameplayGalleryScene>(example::scene_keys::GameplayGallery);
    scene_manager.register_game_scene<example::scene::GameplayVerificationScene>(example::scene_keys::GameplayVerification);
    scene_manager.register_game_scene<example::scene::LocalMultiplayerScene>(
        example::scene_keys::LocalMultiplayer);
    scene_manager.register_game_scene<example::scene::MultiTargetCameraScene>(example::scene_keys::MultiTargetCamera);
    scene_manager.register_game_scene<example::scene::MainMenuScene>(
        example::scene_keys::MainMenu);
    scene_manager.register_game_scene<example::scene::AnimationPreviewScene>(
        example::scene_keys::AnimationPreview);
    scene_manager.register_game_scene<example::scene::PhysicsGalleryScene>(
        example::scene_keys::PhysicsGallery);
    scene_manager.register_game_scene<example::scene::ColliderCombatDemoScene>(
        example::scene_keys::ColliderCombatDemo);
    scene_manager.register_game_scene<example::scene::Box2DLabScene>(
        example::scene_keys::Box2DLab);
    scene_manager.register_game_scene<example::scene::PlatformTileCombatDemoScene>(
        example::scene_keys::PlatformTileCombatDemo);
    scene_manager.register_game_scene<example::scene::TopDownTileCombatDemoScene>(
        example::scene_keys::TopDownTileCombatDemo);
    scene_manager.register_game_scene<example::scene::ShowcaseGalleryScene>(
        example::scene_keys::ShowcaseGallery);
    scene_manager.register_game_scene<example::scene::UiComponentGalleryScene>(
        example::scene_keys::UiComponentGallery);
    scene_manager.register_game_scene<example::scene::EffectsShowcaseScene>(
        example::scene_keys::EffectsShowcase);
}

std::unique_ptr<elysia::tools::IDevelopmentOverlay>
GameModule::create_development_overlay() const
{
#if ELYSIA_ENABLE_IMGUI
    return std::make_unique<elysia::tools::ImGuiDevelopmentOverlay>();
#else
    return {};
#endif
}
}
