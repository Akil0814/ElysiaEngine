#include "example_game_module.h"

#include "../scene/main_menu_scene.h"
#include "../scene/example_scene_keys.h"

#include "../../engine/builtin/builtin_scene_keys.h"
#include "../../engine/builtin/scenes/startup_loading_scene.h"
#include "../../engine/scene/scene_manager.h"

namespace example::application
{
elysia::application::ApplicationDescriptor GameModule::descriptor() const
{
    using elysia::scene::SceneReloadMode;
    using elysia::scene::SceneRoute;
    using elysia::builtin::StartupLoadingScenePayload;
    using elysia::builtin::StartupLogoSlot;

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
                .target = ExampleSceneKeys::MainMenu,
                .payload = example::scene::MainMenuEnterPayload{
                    .replay_theme_music = true
                },
                .reload_mode = SceneReloadMode::Reuse
            },
            .failure_route = std::nullopt,
            .project_logo = StartupLogoSlot{
                .texture_key = "moonline.brand.logo",
                .fade_in_seconds = 1.0,
                .hold_seconds = 1.0,
                .fade_out_seconds = 1.0
            },
            .wait_for_confirmation = true
        },
        .reload_mode = SceneReloadMode::Reuse
    };
    return descriptor;
}

void GameModule::register_scenes(elysia::scene::SceneManager& scene_manager) const
{
    scene_manager.register_game_scene<example::scene::MainMenuScene>(
        ExampleSceneKeys::MainMenu);
}
}
