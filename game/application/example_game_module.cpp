#include "example_game_module.h"

#include "../scene/main_menu_scene.h"
#include "../scene/demo/animation_preview_scene.h"
#include "../scene/demo/demo_gallery_scene.h"
#include "../scene/demo/engine_feature_lab_scene.h"
#include "../scene/demo/ui_component_gallery_scene.h"
#include "../scene/demo/physics/collider_combat_demo_scene.h"
#include "../scene/demo/physics/physics_combat_gallery_scene.h"
#include "../scene/demo/physics/platform_tile_combat_demo_scene.h"
#include "../scene/demo/physics/top_down_tile_combat_demo_scene.h"
#include "../scene/example_scene_keys.h"

#include "../../engine/builtin/builtin_scene_keys.h"
#include "../../engine/builtin/scenes/startup_loading_scene.h"
#include "../../engine/scene/scene_manager.h"
#if ELYSIA_ENABLE_IMGUI
#include "../../engine/tools/imgui/imgui_development_overlay.h"
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
        elysia::ui::UiBuiltinTheme::BlueGlassMoon;
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
                .payload = example::scene::MainMenuEnterPayload{
                    .replay_theme_music = true
                },
                .reload_mode = SceneReloadMode::Reuse
            },
            .failure_route = std::nullopt,
            .project_logo = std::nullopt,
            .wait_for_confirmation = true
        },
        .reload_mode = SceneReloadMode::Reuse
    };
    return descriptor;
}

void GameModule::register_scenes(elysia::scene::SceneManager& scene_manager) const
{
    scene_manager.register_game_scene<example::scene::MainMenuScene>(
        example::scene_keys::MainMenu);
    scene_manager.register_game_scene<example::scene::AnimationPreviewScene>(
        example::scene_keys::AnimationPreview);
    scene_manager.register_game_scene<example::scene::PhysicsCombatGalleryScene>(
        example::scene_keys::PhysicsCombatGallery);
    scene_manager.register_game_scene<example::scene::ColliderCombatDemoScene>(
        example::scene_keys::ColliderCombatDemo);
    scene_manager.register_game_scene<example::scene::PlatformTileCombatDemoScene>(
        example::scene_keys::PlatformTileCombatDemo);
    scene_manager.register_game_scene<example::scene::TopDownTileCombatDemoScene>(
        example::scene_keys::TopDownTileCombatDemo);
    scene_manager.register_game_scene<example::scene::DemoGalleryScene>(
        example::scene_keys::DemoGallery);
    scene_manager.register_game_scene<example::scene::UiComponentGalleryScene>(
        example::scene_keys::UiComponentGallery);
    scene_manager.register_game_scene<example::scene::EngineFeatureLabScene>(
        example::scene_keys::EngineFeatureLab);
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
