#define SDL_MAIN_HANDLED
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/application/lifecycle/application_event_boundary.h"
#include "engine/builtin/scenes/application_failure_presentation.h"
#include "engine/scene/scene_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/ui/core/ui_element.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>

namespace
{
using elysia::tests::require;
using namespace elysia::core;
std::vector<std::string> operations,failures;
std::string throwing_operation;
bool probe(std::string_view operation)
{
    operations.emplace_back(operation);
    if (operation == throwing_operation)
        throw std::runtime_error("injected ordinary operation exception");
    if (std::find(failures.begin(),failures.end(),operation) != failures.end())
    {
        SDL_SetError("injected operation failure");
        return false;
    }
    return true;
}
void baseline(SDL_Renderer* renderer)
{
    detail::render_operation_probe = nullptr;
    require(SDL_SetRenderDrawColor(renderer,10,20,30,40),"baseline color");
    require(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE),"baseline blend");
    const SDL_Rect clip{2,3,20,21};
    require(SDL_SetRenderClipRect(renderer,&clip),"baseline clip");
    operations.clear();
    throwing_operation.clear();
    detail::render_operation_probe = probe;
}
void assert_restored(SDL_Renderer* renderer)
{
    Uint8 r,g,b,a;
    SDL_BlendMode blend;
    SDL_Rect clip;
    require(SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a) && r==10 && g==20 && b==30 && a==40,"color must be restored");
    require(SDL_GetRenderDrawBlendMode(renderer,&blend) && blend==SDL_BLENDMODE_NONE,"blend must be restored");
    require(SDL_GetRenderClipRect(renderer,&clip) && SDL_RenderClipEnabled(renderer)
        && clip.x==2 && clip.y==3 && clip.w==20 && clip.h==21,"clip must be restored");
}
class RectProbe final : public elysia::ui::UiElement
{
public:
    void submit_ui_render_commands(std::vector<UiRenderCommand>& out) const override
    {
        UiRenderCommand command;
        command.type = UiRenderCommandType::FillRect;
        command.screen_rect = {0,0,10,10};
        command.color = {255,0,0,255};
        out.push_back(command);
    }
};
class RenderScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override { (void)create_and_add_object<RectProbe>(); }
    void on_exit() override { ++exits; }
    void on_reset() override {}
    static inline int exits = 0;
};
class TaggedScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        origin = std::source_location::current();
        throw elysia::scene::SceneBoundaryRuntimeError(elysia::scene::SceneBoundary::Enter,"tagged fault",origin);
    }
    void on_exit() override {}
    void on_reset() override {}
    static inline std::source_location origin;
};
class UntaggedScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        origin = std::source_location::current();
        throw std::runtime_error("ordinary fault");
    }
    void on_exit() override {}
    void on_reset() override {}
    static inline std::source_location origin;
};
class TransitionBackendScene final : public elysia::scene::Scene
{
public:
    TransitionBackendScene(elysia::scene::SceneBoundary boundary,bool& armed)
        : _boundary(boundary),_armed(armed) {}
    void on_enter(const elysia::scene::ScenePayload&) override { fail_at(elysia::scene::SceneBoundary::Enter); }
    void on_exit() override { fail_at(elysia::scene::SceneBoundary::Exit); }
    void on_reset() override { fail_at(elysia::scene::SceneBoundary::Reset); }
    void on_runtime_attach() override { fail_at(elysia::scene::SceneBoundary::Attach); }
    void on_runtime_detach() override { fail_at(elysia::scene::SceneBoundary::Detach); }
private:
    void fail_at(elysia::scene::SceneBoundary boundary)
    {
        if (_armed && boundary == _boundary)
            throw RenderBackendError({"transition render",make_failure_diagnostic("transition backend failure")});
    }
    elysia::scene::SceneBoundary _boundary;
    bool& _armed;
};

class RetirementBackendScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override { _object = create_and_add_object<RectProbe>(); }
    void on_exit() override {}
    void on_reset() override {}
    void on_after_update(double) override { _object->destroy(); }
    void on_scene_object_removing(elysia::core::SceneObject&) override
    {
        throw RenderBackendError({"retirement render",make_failure_diagnostic("retirement backend failure")});
    }
private:
    RectProbe* _object = nullptr;
};

void test_transition_backend_failures(SDL_Renderer* renderer)
{
    using namespace elysia::scene;
    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(renderer,registry,64,64);
    auto* termination = elysia::tools::TerminationManager::instance();
    for (SceneBoundary boundary : {SceneBoundary::Enter,SceneBoundary::Attach,SceneBoundary::Reset,
        SceneBoundary::Exit,SceneBoundary::Detach})
    {
        SceneManager manager;
        int recoveries = 0;
        bool armed = boundary == SceneBoundary::Enter || boundary == SceneBoundary::Attach;
        manager.initialize(context,[&](const auto&) { ++recoveries; return SceneRoute{.target=2}; });
        manager.register_game_scene<TransitionBackendScene>(1,boundary,std::ref(armed));
        manager.register_game_scene<RenderScene>(2);
        termination->reset_for_testing();
        const bool result = elysia::application::run_event_boundary("transition",[&] {
            manager.start({.target=1});
            armed = true;
            manager.on_scene_request({.type=SceneRequestType::Switch,
                .route={.target=boundary == SceneBoundary::Reset ? 1u : 2u,
                    .reload_mode=boundary == SceneBoundary::Reset ? SceneReloadMode::Reset : SceneReloadMode::Reuse}});
            manager.on_update(0);
        });
        const auto info = termination->termination_info();
        require(!result && recoveries==0 && info
            && info->reason==elysia::tools::TerminationReason::FatalRuntimeFailure,
            "transition backend failures must bypass failure scenes after transactional cleanup");
        armed = false;
        require(manager.shutdown(),"backend failure during transition must leave shutdown clean");
    }
    SceneManager recovery_manager;
    bool armed = true;
    int recoveries = 0;
    recovery_manager.initialize(context,[&](const auto&) { ++recoveries; return SceneRoute{.target=2}; });
    recovery_manager.register_game_scene<TaggedScene>(1);
    recovery_manager.register_game_scene<TransitionBackendScene>(2,SceneBoundary::Enter,std::ref(armed));
    termination->reset_for_testing();
    require(!elysia::application::run_event_boundary("recovery",[&] { recovery_manager.start({.target=1}); })
        && recoveries==1 && termination->termination_info()->category=="render",
        "backend failure in a recovery scene must reach the application boundary without recursion");
    require(recovery_manager.shutdown(),"backend recovery failure must leave shutdown clean");
    SceneManager retirement_manager;
    recoveries = 0;
    retirement_manager.initialize(context,[&](const auto&) { ++recoveries; return SceneRoute{.target=2}; });
    retirement_manager.register_game_scene<RetirementBackendScene>(1);
    retirement_manager.start({.target=1});
    termination->reset_for_testing();
    require(!elysia::application::run_event_boundary("update",[&] { retirement_manager.on_update(0); })
        && recoveries==0 && termination->termination_info()->category=="render",
        "object retirement must preserve backend error type after completing ordered cleanup");
    require(retirement_manager.shutdown(),"backend retirement failure must still retire the object completely");
    termination->reset_for_testing();
}
class Observer final : public elysia::scene::SceneManagerObserver
{
public:
    void on_scene_manager_quit_requested() override {}
    void on_scene_manager_fault(const elysia::scene::SceneBoundaryFailure& failure) override
    {
        ++calls;
        saved = failure;
        if (throws) throw 42;
    }
    int calls = 0;
    bool throws = false;
    elysia::scene::SceneBoundaryFailure saved;
};
void test_scene_diagnostics()
{
    using namespace elysia::scene;
    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(nullptr,registry,1280,720);
    SceneManager manager;
    auto* logger = elysia::tools::Logger::instance();
    elysia::tools::LoggerConfig log_config;
    log_config.file_mode = elysia::tools::LogFileMode::Disabled;
    log_config.console_color_mode = elysia::tools::ConsoleColorMode::Never;
    require(logger->configure(log_config),"scene diagnostic logger fixture");
    logger->initialize();
    std::ostringstream logs;
    auto* previous = std::clog.rdbuf(logs.rdbuf());
    manager.initialize(context);
    manager.register_game_scene<TaggedScene>(19);
    Observer first,last;
    first.throws = true;
    manager.attach(&first); manager.attach(&last);
    manager.start({.target=19});
    require(first.calls==1 && last.calls==1 && manager.state()==SceneManagerState::Faulted,"throwing observer must not block later fault delivery");
    require(last.saved.diagnostic.origin.line()==TaggedScene::origin.line()
        && last.saved.diagnostic.origin.file_name()==TaggedScene::origin.file_name(),"tagged exception origin must survive capture");
    const auto route = elysia::builtin::make_application_failure_route(last.saved);
    const auto* payload = try_scene_payload<elysia::builtin::ApplicationFailureScenePayload>(route.payload);
    require(payload && payload->diagnostic.entries.front().subject_key=="19"
        && payload->diagnostic.entries.front().reason=="Enter","route must retain scene and boundary");
    const auto report = elysia::builtin::build_application_failure_presentation(*payload,"test.log",true);
    const auto diagnostic_text = format_failure_diagnostic(last.saved.diagnostic,"APPLICATION-FATAL","scene");
    const auto source = std::string(source_file_basename(last.saved.diagnostic.origin))
        + ':' + std::to_string(last.saved.diagnostic.origin.line());
    require(logs.str().find(diagnostic_text)!=std::string::npos
        && report.copy_report.find("Source: " + source)!=std::string::npos
        && report.copy_report.find(report.diagnostic_details)!=std::string::npos,
        "logs and copied report must contain the same scene diagnostic including its origin");
    require(report.copy_report.find("19")!=std::string::npos && report.copy_report.find("Enter")!=std::string::npos
        && report.copy_report.find("tagged fault")!=std::string::npos,"copy report must retain scene diagnostics");
    require(manager.shutdown(),"faulted scene must close");
    const SceneBoundaryFailure manual_failure{19,SceneBoundary::Input,
        make_failure_diagnostic("manual failure",{make_failure_diagnostic_entry("asset","custom")},TaggedScene::origin)};
    const auto manual_route = elysia::builtin::make_application_failure_route(manual_failure);
    const auto* manual_payload = try_scene_payload<elysia::builtin::ApplicationFailureScenePayload>(manual_route.payload);
    require(manual_payload && manual_payload->diagnostic.entries.size()==2
        && manual_payload->diagnostic.entries[0].subject_key=="custom"
        && manual_payload->diagnostic.entries[1].subject_key=="19"
        && manual_payload->diagnostic.entries[1].reason=="Input"
        && manual_payload->diagnostic.origin.line()==TaggedScene::origin.line(),
        "direct scene failure conversion must preserve explicit scene fields and existing diagnostic entries");
    SceneManager ordinary_manager;
    ordinary_manager.initialize(context);
    ordinary_manager.register_game_scene<UntaggedScene>(20);
    ordinary_manager.attach(&last);
    ordinary_manager.start({.target=20});
    require(last.saved.diagnostic.message=="ordinary fault"
        && std::string_view(last.saved.diagnostic.origin.file_name()).find("scene_manager.cpp")!=std::string_view::npos
        && std::string_view(last.saved.diagnostic.origin.file_name())!=UntaggedScene::origin.file_name(),
        "ordinary exceptions must report the catch site, not an invented throw site");
    require(ordinary_manager.shutdown(),"ordinary fault must close");
    std::clog.rdbuf(previous);
    logger->shutdown();
}
}
int main()
{
    require(SDL_Init(SDL_INIT_VIDEO),"video fixture");
    SDL_Window* window = SDL_CreateWindow("error boundaries",64,64,SDL_WINDOW_HIDDEN);
    require(window,"window fixture");
    SDL_Renderer* renderer = SDL_CreateRenderer(window,"software");
    require(renderer,"renderer fixture");
    UiRenderCommand command;
    command.type = UiRenderCommandType::FillRect;
    command.screen_rect = {0,0,10,10};
    command.color = {255,0,0,255};
    require(!execute_render_command(nullptr,command),"missing renderer must fail explicitly");
    for (const char* operation : {"SDL_GetRenderClipRect","SDL_GetRenderDrawBlendMode","SDL_GetRenderDrawColor",
        "SDL_SetRenderClipRect","SDL_SetRenderDrawBlendMode","SDL_SetRenderDrawColor","SDL_RenderFillRect"})
    {
        baseline(renderer); failures = {operation};
        const auto result = execute_render_commands(renderer,std::vector{command,command});
        require(!result && result.error().operation==operation,"SDL failure must retain the first operation");
        require(std::count(operations.begin(),operations.end(),"SDL_RenderFillRect") <= 1,"batch must stop after first failed command");
        assert_restored(renderer);
    }
    baseline(renderer); failures = {"SDL_RenderFillRect","restore.SDL_SetRenderDrawColor"};
    const auto multiple = execute_render_command(renderer,command);
    require(!multiple && multiple.error().operation=="SDL_RenderFillRect" && multiple.error().diagnostic.entries.size()==1,
        "drawing failure and restoration failure must both survive");
    require(std::find(operations.begin(),operations.end(),"restore.SDL_SetRenderClipRect")!=operations.end(),"one restoration failure must not block remaining restoration");
    baseline(renderer); failures={"SDL_RenderFillRect"}; throwing_operation="restore.SDL_SetRenderDrawColor";
    const auto ordinary_restore = execute_render_command(renderer,command);
    require(!ordinary_restore && ordinary_restore.error().operation=="SDL_RenderFillRect"
        && ordinary_restore.error().diagnostic.entries[0].reason=="injected ordinary operation exception"
        && operations.back()=="restore.SDL_SetRenderClipRect",
        "ordinary restore exceptions must not hide an earlier backend failure or stop remaining restores");
    baseline(renderer); failures={"restore.SDL_SetRenderDrawColor"}; throwing_operation="SDL_RenderFillRect";
    const auto backend_restore = execute_render_command(renderer,command);
    require(!backend_restore && backend_restore.error().operation=="restore.SDL_SetRenderDrawColor"
        && backend_restore.error().diagnostic.entries[0].subject_type=="render-command",
        "backend restoration failures must retain command exceptions and force fatal backend routing");
    baseline(renderer); failures.clear(); throwing_operation="SDL_RenderFillRect";
    bool ordinary_propagated = false;
    try { (void)execute_render_command(renderer,command); }
    catch (const std::runtime_error& error) { ordinary_propagated = std::string_view(error.what())=="injected ordinary operation exception"; }
    require(ordinary_propagated,"ordinary command exceptions with successful restoration must keep scene exception semantics");
    assert_restored(renderer);
    for (const char* operation : {"restore.SDL_SetRenderDrawColor","restore.SDL_SetRenderDrawBlendMode","restore.SDL_SetRenderClipRect"})
    {
        baseline(renderer); failures={operation};
        const auto result=execute_render_command(renderer,command);
        require(!result && result.error().operation==operation,"restoration alone must still report render failure");
    }
    UiRenderCommand stroke=command; stroke.type=UiRenderCommandType::DrawRect;
    for (const char* operation : {"SDL_GetRenderScale","SDL_GetRenderLogicalPresentation","SDL_GetRenderLogicalPresentationRect","SDL_GetRenderViewport","SDL_RenderGeometry"})
    {
        baseline(renderer); failures={operation};
        const auto result=execute_render_command(renderer,stroke);
        require(!result && result.error().operation==operation,"stroke queries and geometry must propagate failure");
        assert_restored(renderer);
    }
    baseline(renderer); failures={"filledCircleRGBA"};
    UiRenderCommand circle=command; circle.type=UiRenderCommandType::FillCircle; circle.circle_center={10,10}; circle.circle_radius=5;
    require(!execute_render_command(renderer,circle),"gfx primitive failure must propagate");
    SDL_Texture* texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,4,4);
    require(texture,"texture fixture");
    UiRenderCommand textured=command; textured.type=UiRenderCommandType::Texture; textured.texture=texture;
    textured.alpha=64; textured.texture_color_modulation=TextureColorModulation{100,120,140};
    for (const char* operation : {"SDL_GetTextureAlphaMod","SDL_GetTextureColorMod","SDL_SetTextureAlphaMod","SDL_SetTextureColorMod","SDL_RenderTextureRotated"})
    {
        baseline(renderer);
        require(SDL_SetTextureAlphaMod(texture,200) && SDL_SetTextureColorMod(texture,10,20,30),"texture baseline");
        failures={operation};
        const auto result=execute_render_command(renderer,textured);
        require(!result && result.error().operation==operation,"texture failure must propagate");
        Uint8 r,g,b,a;
        require(SDL_GetTextureAlphaMod(texture,&a) && a==200 && SDL_GetTextureColorMod(texture,&r,&g,&b)
            && r==10 && g==20 && b==30,"texture state must survive failure");
    }
    for (const char* operation : {"restore.SDL_SetTextureAlphaMod","restore.SDL_SetTextureColorMod"})
    {
        baseline(renderer);
        require(SDL_SetTextureAlphaMod(texture,200) && SDL_SetTextureColorMod(texture,10,20,30),"texture restore baseline");
        failures={operation};
        require(!execute_render_command(renderer,textured),"texture restoration failure must propagate");
    }
    for (const char* operation : {"SDL_RenderClear","SDL_RenderPresent"})
    {
        baseline(renderer); failures={operation};
        const auto result = std::string_view(operation)=="SDL_RenderClear" ? begin_render_frame(renderer) : present_render_frame(renderer);
        require(!result && result.error().operation==operation,"frame failures must propagate");
    }
    // A backend failure must bypass the scene recovery route.
    baseline(renderer); failures={"SDL_RenderFillRect"};
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(renderer,registry,64,64);
    elysia::scene::SceneManager manager;
    int recoveries=0;
    manager.initialize(context,[&](const auto&) { ++recoveries; return elysia::scene::SceneRoute{.target=1}; });
    manager.register_game_scene<RenderScene>(1); manager.start({.target=1});
    auto* termination=elysia::tools::TerminationManager::instance(); termination->reset_for_testing();
    require(!elysia::application::run_event_boundary("render",[&] { manager.on_render(renderer); }),"backend error must reach application boundary");
    require(recoveries==0 && termination->termination_info()->reason==elysia::tools::TerminationReason::FatalRuntimeFailure,
        "backend error must request fatal exit without recovery");
    require(manager.shutdown() && RenderScene::exits==1,"fatal render path must still close the scene");
    termination->reset_for_testing();
    detail::render_operation_probe=nullptr;
    test_transition_backend_failures(renderer);
    test_scene_diagnostics();
    SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
}
