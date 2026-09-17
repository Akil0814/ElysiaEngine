#include "engine/gameplay/control/controller_service.h"
#include "tests/support/input_snapshot_builder.h"
#define SDL_MAIN_HANDLED
#include "engine/camera/camera_manager.h"
#include "engine/core/render/render_command.h"
#include "engine/core/render/debug_draw_projection.h"
#include "engine/core/render/render_command_projection.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/object_query/game_object_query_service.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/scene/scene_manager.h"
#include "game/scene/demo/physics/box2d_lab_scene.h"
#include "game/demo/physics/physics_scenario_presentation.h"
#include "game/scene/example_scene_keys.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
int main(int argc, char **argv)
{
    using elysia::tests::require;
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    require(SDL_Init(SDL_INIT_VIDEO), "SDL initializes");
    SDL_Surface *surface = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr, "Software renderer creates");
    {
        elysia::io::ContentRegistry registry;
        elysia::scene::SceneRuntimeContext context(renderer, registry, 1280, 720);
        elysia::scene::SceneManager scene;
        scene.register_game_scene<example::scene::Box2DLabScene>(example::scene_keys::Box2DLab);
        scene.set_runtime_context(context);
        if (!elysia::gameplay::ControllerService::instance()->session_active())
            (void)elysia::gameplay::ControllerService::instance()->begin_session();
        scene.start({.target = example::scene_keys::Box2DLab,
                     .payload = example::scene::DemoScenePayload{
                         .return_route = {.target = example::scene_keys::PhysicsCombatGallery}}});
        using example::demo::physics::PhysicsScenarioPresentation;
        auto find_presentation=[](){
            for(auto* o:ELYSIA_OBJECT_QUERY->find_objects<>())
                if(auto* p=dynamic_cast<PhysicsScenarioPresentation*>(o))return p;
            return static_cast<PhysicsScenarioPresentation*>(nullptr);
        };
        auto* presentation=find_presentation();
        require(presentation!=nullptr,"Lab exposes the shared scenario presentation");
        scene.on_update(1.0/60);
        require(presentation->scenario().result().steps==0,"Lab waits for explicit run");
        scene.on_input(
            elysia::tests::events_snapshot({{.control = elysia::input::RawInputControl::KeyN,
                                                .type = elysia::input::RawInputEventType::ControlPressed}}));
        scene.on_update(1.0/60);
        require(presentation->scenario().result().steps==1&&presentation->scenario().paused(),"N executes one paused tick");
        scene.on_update(1.0);
        require(presentation->scenario().result().steps==1,"Pause prevents catch-up simulation");
        scene.on_input(
            elysia::tests::events_snapshot({{.control = elysia::input::RawInputControl::KeyP,
                                                .type = elysia::input::RawInputEventType::ControlPressed}}));
        for(int i=0;i<120;++i)scene.on_update(1.0/60);
        require(presentation->scenario().result().status==example::demo::physics::ScenarioStatus::Passed,
                "Scene runs the same behavioral checks as the headless runner");
        auto objects = ELYSIA_OBJECT_QUERY->find_objects<>();
        const auto& world=presentation->scenario().world();
        require(world.registered_object_count()>0,"Shared scenario owns live physical bodies");
        std::vector<elysia::core::RenderCommand> commands;
        for (auto *object : objects)
            object->submit_render_commands(commands);
        require(!commands.empty(), "Scenario geometry is submitted");
        std::vector<elysia::core::ScreenRenderCommand> screen;
        elysia::core::project_render_commands_to_screen(
            commands,
            elysia::camera::CameraManager::instance()->camera(elysia::camera::CameraSlot::Main),
            screen);
        SDL_SetRenderDrawColor(renderer, 24, 30, 42, 255);
        SDL_RenderClear(renderer);
        elysia::core::execute_render_commands(renderer, screen);
        auto *debug = elysia::tools::DebugDraw::instance();
        std::vector<elysia::core::UiRenderCommand> overlay;
        elysia::core::append_projected_debug_draw_commands(debug->commands(), debug->enabled_categories(),
            elysia::camera::CameraManager::instance()->camera(elysia::camera::CameraSlot::Main), overlay);
        require(!overlay.empty(), "Debug overlay projects into the preview");
        elysia::core::execute_render_commands(renderer, overlay);
        SDL_RenderPresent(renderer);
        if (argc > 1)
            require(IMG_SavePNG(surface, argv[1]), "Lab preview saved");
        scene.on_input(
            elysia::tests::events_snapshot({{.control = elysia::input::RawInputControl::KeyR,
                                                .type = elysia::input::RawInputEventType::ControlPressed}}));
        scene.on_update(1.0 / 60);
        presentation=find_presentation();
        require(presentation&&presentation->scenario().result().steps==0,
                "R recreates the fixture and clears the old result");
        scene.shutdown();
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    SDL_Quit();
    std::cout << "Box2D lab scene smoke passed\n";
}
