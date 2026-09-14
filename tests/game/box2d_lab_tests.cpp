#define SDL_MAIN_HANDLED
#include "engine/camera/camera_manager.h"
#include "engine/core/render/render_command.h"
#include "engine/core/render/render_command_projection.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/object_query/game_object_query_service.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/scene/scene_manager.h"
#include "game/scene/demo/physics/box2d_lab_scene.h"
#include "game/scene/example_scene_keys.h"
#include "tests/support/test_assertions.h"
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
int main(int argc, char **argv)
{
    using elysia::tests::require;
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    require(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL initializes");
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr, "Software renderer creates");
    {
        elysia::io::ContentRegistry registry;
        elysia::scene::SceneRuntimeContext context(renderer, registry, 1280, 720);
        elysia::scene::SceneManager scene;
        scene.register_game_scene<example::scene::Box2DLabScene>(example::scene_keys::Box2DLab);
        scene.set_runtime_context(context);
        scene.start({.target = example::scene_keys::Box2DLab,
                     .payload = example::scene::DemoScenePayload{
                         .return_route = {.target = example::scene_keys::PhysicsCombatGallery}}});
        for (int i = 0; i < 300; ++i)
            scene.on_update(1.0 / 60);
        auto objects = ELYSIA_OBJECT_QUERY->find_objects<>();
        elysia::physics::PhysicsWorld *world = nullptr;
        for (auto *object : objects)
            if (auto *p = dynamic_cast<elysia::physics::PhysicsParticipant *>(object))
            {
                world = p->physics_world();
                break;
            }
        require(world && world->registered_object_count() > 20, "Lab registers physical bodies");
        require(world->last_step_stats().joints == 3, "Lab registers spring, pendulum and motor");
        std::vector<elysia::core::RenderCommand> commands;
        for (auto *object : objects)
            object->submit_render_commands(commands);
        require(commands.size() > 40, "Rotated geometry is submitted");
        std::vector<elysia::core::ScreenRenderCommand> screen;
        elysia::core::project_render_commands_to_screen(
            commands,
            elysia::camera::CameraManager::instance()->camera(elysia::camera::CameraSlot::Main),
            screen);
        SDL_SetRenderDrawColor(renderer, 24, 30, 42, 255);
        SDL_RenderClear(renderer);
        elysia::core::execute_render_commands(renderer, screen);
        SDL_RenderPresent(renderer);
        if (argc > 1)
            require(IMG_SavePNG(surface, argv[1]) == 0, "Lab preview saved");
        scene.on_input({}, {{.control = elysia::input::RawInputControl::KeySpace,
                             .type = elysia::input::RawInputEventType::ControlPressed}});
        scene.on_update(1.0 / 60);
        require(world->last_step_stats().awake_bodies > 10, "Space wakes and kicks the stack");
        scene.shutdown();
    }
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
    std::cout << "Box2D lab scene smoke passed\n";
}
