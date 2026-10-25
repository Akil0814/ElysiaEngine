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
#include "game/showcase/physics/box2d_lab_scene.h"
#include "game/showcase/scenarios/showcase_scenario_presentation.h"
#include "game/navigation/showcase_scene_keys.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <functional>
#include "tests/support/scene_test_access.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/core/ui_child_host.h"
// Observe the real ScenarioScene lifecycle without adding a production scene accessor.
class InspectableLabScene final : public example::scene::ScenarioScene
{
public:
    explicit InspectableLabScene(example::scene::ScenarioScene** active)
        : ScenarioScene(example::scene_keys::Box2DLab,false) { *active=this; }
};
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
        scene.initialize(context);
        example::scene::ScenarioScene* active_lab=nullptr;
        scene.register_game_scene<InspectableLabScene>(example::scene_keys::Box2DLab,&active_lab);
        if (!elysia::gameplay::ControllerService::instance()->session_active())
            (void)elysia::gameplay::ControllerService::instance()->begin_session();
        scene.start({.target = example::scene_keys::Box2DLab,
                     .payload = example::scene::ScenarioEnterPayload{
                         .return_route = {.target = example::scene_keys::PhysicsGallery},.scenario_id=argc>2?argv[2]:"motion"}});
        using example::showcase::scenarios::ShowcaseScenarioPresentation;
        auto find_presentation=[](){
            for(auto* o:ELYSIA_OBJECT_QUERY->find_objects<>())
                if(auto* p=dynamic_cast<ShowcaseScenarioPresentation*>(o))return p;
            return static_cast<ShowcaseScenarioPresentation*>(nullptr);
        };
        auto* presentation=find_presentation();
        require(presentation!=nullptr,"Lab exposes the shared scenario presentation");
        scene.on_update(1.0/60);
        require(presentation->scenario().result().steps==0,"Lab waits for explicit run");
        using DrawCategory=elysia::tools::DebugDrawCategory;
        auto* debug_controls=elysia::tools::DebugDraw::instance();
        require(!debug_controls->is_enabled(DrawCategory::PhysicsJointDetails)&&!debug_controls->is_enabled(DrawCategory::PhysicsChainDetails)&&!debug_controls->is_enabled(DrawCategory::PhysicsTarget),"Optional details start disabled");
        auto shortcut=[&](elysia::input::RawInputControl key){for(auto phase:{elysia::input::RawInputEventType::ControlPressed,elysia::input::RawInputEventType::ControlReleased})scene.on_input(elysia::tests::events_snapshot({{.control=key,.type=phase}}));scene.on_update(0);};

        scene.on_input(
            elysia::tests::events_snapshot({{.control = elysia::input::RawInputControl::KeyN,
                                                .type = elysia::input::RawInputEventType::ControlPressed}}));
        scene.on_update(1.0/60);
        require(presentation->scenario().result().steps==1&&presentation->scenario().paused(),"N executes one paused tick");
        scene.on_update(1.0);
        require(presentation->scenario().result().steps==1,"Pause prevents catch-up simulation");
        const auto detail_keys={elysia::input::RawInputControl::KeyF2,elysia::input::RawInputControl::KeyF3,elysia::input::RawInputControl::KeyF4};
        for(auto key:detail_keys)shortcut(key);
        const auto& detailed=presentation->scenario().world();
        require(debug_controls->is_enabled(DrawCategory::PhysicsJointDetails|DrawCategory::PhysicsChainDetails|DrawCategory::PhysicsTarget)&&elysia::physics::captures_physics_debug(detailed.debug_capture(),elysia::physics::PhysicsDebugCapture::Chains|elysia::physics::PhysicsDebugCapture::Targets),"F2/F3/F4 independently enable snapshot capture while paused");
        if(argc>2&&std::string_view(argv[2])=="edges")require(detailed.debug_snapshot().chains.size()==2,"F3 exposes actual chain data");
        if(argc>2&&std::string_view(argv[2])=="target_pose")require(detailed.debug_snapshot().targets.size()==1,"F4 reveals the last completed target without another physics step");
        for(auto key:detail_keys)shortcut(key);
        require(!elysia::physics::captures_physics_debug(detailed.debug_capture(),elysia::physics::PhysicsDebugCapture::Chains)&&!elysia::physics::captures_physics_debug(detailed.debug_capture(),elysia::physics::PhysicsDebugCapture::Targets),"Disabling detail keys drops optional capture");
        for(const auto& command:debug_controls->commands())require(command.category!=DrawCategory::PhysicsJointDetails&&command.category!=DrawCategory::PhysicsChainDetails&&command.category!=DrawCategory::PhysicsTarget,"Disabled categories leave no retained commands");
        for(auto key:detail_keys)shortcut(key);
        shortcut(elysia::input::RawInputControl::KeyF1);require(detailed.debug_capture()==elysia::physics::PhysicsDebugCapture::None&&debug_controls->commands().empty(),"F1 clears all detail capture and commands");shortcut(elysia::input::RawInputControl::KeyF1);
        require(presentation->scenario().result().steps==1,"Debug controls never advance the paused simulation");

        scene.on_input(
            elysia::tests::events_snapshot({{.control = elysia::input::RawInputControl::KeyP,
                                                .type = elysia::input::RawInputEventType::ControlPressed}}));
        for(unsigned i=0;i<presentation->scenario().descriptor().max_steps;++i)scene.on_update(1.0/60);
        require(presentation->scenario().result().status==example::showcase::scenarios::ScenarioStatus::Passed,
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
        elysia::core::require_render_success(elysia::core::execute_render_commands(renderer, screen));
        auto *debug = elysia::tools::DebugDraw::instance();
        std::vector<elysia::core::UiRenderCommand> overlay;
        elysia::core::append_projected_debug_draw_commands(debug->commands(), debug->enabled_categories(),
            elysia::camera::CameraManager::instance()->camera(elysia::camera::CameraSlot::Main), overlay);
        require(!overlay.empty(), "Debug overlay projects into the preview");
        elysia::core::require_render_success(elysia::core::execute_render_commands(renderer, overlay));
        SDL_RenderPresent(renderer);
        if (argc > 1 && std::string_view(argv[1])!="-")
            require(IMG_SavePNG(surface, argv[1]), "Lab preview saved");
        if (argc>2 && (std::string_view(argv[2])=="joints"||std::string_view(argv[2]).starts_with("joint_")))
        {
            std::function<elysia::ui::UiButton*(elysia::ui::UiElement&)> find_control;
            find_control=[&](elysia::ui::UiElement& element)->elysia::ui::UiButton* {
                if(auto* button=dynamic_cast<elysia::ui::UiButton*>(&element);
                    button&&button->text_content().value=="physics_tests.control_joints") return button;
                if(auto* host=dynamic_cast<elysia::ui::UiChildHost*>(&element))
                    for(std::size_t i=0;i<host->child_count();++i)
                        if(auto* found=find_control(*host->child_at(i)))return found;
                return nullptr;
            };
            auto* root=elysia::scene::SceneTestAccess::ui_root(*active_lab,0);
            auto* button=find_control(*root);
            require(button,"Joint scene exposes the runtime control button");
            const auto point=button->presentation_screen_rect().center();
            for(auto phase:{elysia::input::RawInputEventType::ControlPressed,elysia::input::RawInputEventType::ControlReleased})
                scene.on_input(elysia::tests::events_snapshot({{
                    .control=elysia::input::RawInputControl::MouseLeft,.type=phase,
                    .device=elysia::input::InputDevice::Mouse,.mouse_x=int(point.x),.mouse_y=int(point.y)}}));
            scene.on_update(1.0/60);
            require(presentation->scenario().result().status==example::showcase::scenarios::ScenarioStatus::Running
                &&presentation->scenario().world().last_step_stats().joints==(std::string_view(argv[2])=="joints"?3u:std::string_view(argv[2])=="joint_filter"?0u:1u),
                "Clicking controls after verification starts a fresh live joint simulation");
            scene.on_update(1.0/60);
            if(std::string_view(argv[2])=="joint_mouse")
            {
                using namespace elysia::input;
                auto& camera=elysia::camera::CameraManager::instance()->camera(elysia::camera::CameraSlot::Main);
                auto target=camera.world_to_screen(elysia::core::Vector2{600,320});
                scene.on_input(elysia::tests::events_snapshot({{.control=RawInputControl::MouseLeft,.type=RawInputEventType::ControlPressed,.device=InputDevice::Mouse,.mouse_x=int(target.x),.mouse_y=int(target.y)}}));
                target.x+=20;
                scene.on_input(elysia::tests::events_snapshot({{.type=RawInputEventType::MouseMoved,.device=InputDevice::Mouse,.mouse_x=int(target.x),.mouse_y=int(target.y)}}));
                scene.on_input(elysia::tests::events_snapshot({{.control=RawInputControl::MouseLeft,.type=RawInputEventType::ControlReleased,.device=InputDevice::Mouse,.mouse_x=int(target.x),.mouse_y=int(target.y)}}));
                scene.on_update(1.0/60);
                const auto& snapshot=presentation->scenario().world().debug_snapshot();
                require(snapshot.joints.size()==1,"Mouse drag keeps its joint alive");
                const auto state=presentation->scenario().world().joint_state(snapshot.joints[0].handle);
                const auto desired=camera.screen_to_world(elysia::core::Vector2{float(int(target.x)),float(int(target.y))});
                require((std::get<elysia::physics::MouseJointState>(state->details).settings.target-desired).length()<2,"Actual scene pointer input updates the world target");
            }
            for(int i=0;i<240;++i)scene.on_update(1.0/60);
            require(presentation->scenario().result().status==example::showcase::scenarios::ScenarioStatus::Running,"Runtime controls keep continuous manual simulation active");
        }
        scene.on_input(
            elysia::tests::events_snapshot({{.control = elysia::input::RawInputControl::KeyR,
                                                .type = elysia::input::RawInputEventType::ControlPressed}}));
        scene.on_update(1.0 / 60);
        presentation=find_presentation();
        require(presentation&&presentation->scenario().result().steps==0,
                "R recreates the fixture and clears the old result");
        require(!debug_controls->is_enabled(DrawCategory::PhysicsJointDetails)&&!debug_controls->is_enabled(DrawCategory::PhysicsChainDetails)&&!debug_controls->is_enabled(DrawCategory::PhysicsTarget),"Reentry restores the default detail selection");
        scene.shutdown();
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    SDL_Quit();
    std::cout << "Box2D lab scene smoke passed\n";
}
