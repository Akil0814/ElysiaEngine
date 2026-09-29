#include "engine/application/application.h"
#include "engine/application/lifecycle/frame_pacing.h"
#include "engine/core/render/sdl_render_boundary.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/tools/termination_manager.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
using elysia::tests::require;
std::string mode;
int exits = 0, destroyed = 0, overlay_stops = 0;
std::vector<double> frames;
std::atomic<bool> updated = false;
class RenderProbe final : public elysia::ui::UiElement
{
public:
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const override
    {
        elysia::core::UiRenderCommand command;
        command.type = elysia::core::UiRenderCommandType::FillRect;
        command.screen_rect = {0,0,10,10};
        command.color = {255,0,0,255};
        out.push_back(command);
    }
};
class ProbeScene final : public elysia::scene::Scene
{
public:
    ~ProbeScene() override { ++destroyed; }
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        if (mode == "init_backend")
            throw elysia::core::RenderBackendError({"initial scene render",
                elysia::core::make_failure_diagnostic("injected initial scene backend failure")});
        if (mode == "render_draw") (void)create_and_add_object<RenderProbe>();
    }
    void on_reset() override {}
    void on_runtime_detach() override
    {
        if (mode=="cleanup_backend")
            throw elysia::core::RenderBackendError({"cleanup detach",
                elysia::core::make_failure_diagnostic("backend detach after update failure")});
    }
    void on_exit() override
    {
        ++exits;
        if (mode == "exit_standard") throw std::runtime_error("injected exit failure");
        if (mode == "exit_unknown") throw 42;
    }
    void on_after_update(double delta) override
    {
        updated = true;
        frames.push_back(delta);
        if (mode=="cleanup_backend") throw std::runtime_error("primary update before backend cleanup");
        if (mode.starts_with("exit_") || (mode == "timing" && frames.size() >= 190))
            request_quit();
    }
};
class ThrowingOverlay final : public elysia::tools::IDevelopmentOverlay
{
public:
    std::expected<void,std::string> initialize(SDL_Window&,SDL_Renderer&) override { return {}; }
    void process_event(const SDL_Event&) override { throw std::runtime_error("injected event failure"); }
    void begin_frame(double) override {}
    void render(SDL_Renderer&) override {}
    void shutdown() noexcept override { ++overlay_stops; }
    elysia::input::InputCapture captured_input() const noexcept override
    { return elysia::input::InputCapture::None; }
    elysia::tools::DevelopmentPanelHandle register_panel(std::string,DrawCallback) override { return {}; }
    bool unregister_panel(elysia::tools::DevelopmentPanelHandle) override { return true; }
};
class Module final : public elysia::application::IGameModule
{
public:
    elysia::application::ApplicationDescriptor descriptor() const override
    {
        elysia::application::ApplicationDescriptor result;
        result.initial_route.target = 1;
        return result;
    }
    void register_scenes(elysia::scene::SceneManager& manager) const override
    {
        manager.register_game_scene<ProbeScene>(1);
    }
    std::unique_ptr<elysia::tools::IDevelopmentOverlay> create_development_overlay() const override
    { return mode == "event" ? std::make_unique<ThrowingOverlay>() : nullptr; }
};
}
int main(int argc,char** argv)
{
    mode = argc > 1 ? argv[1] : "exit_standard";
    if (mode.starts_with("manager_"))
    {
        mode = mode == "manager_standard" ? "exit_standard" : "exit_unknown";
        elysia::io::ContentRegistry registry;
        elysia::scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
        elysia::scene::SceneManager manager;
        manager.initialize(context);
        manager.register_game_scene<ProbeScene>(1);
        manager.start({.target = 1});
        require(!manager.shutdown() && !manager.shutdown(),"cleanup failure must be sticky");
        require(exits == 1 && destroyed == 1,"exit exactly once and destroy on failure");
        bool restart_rejected = false;
        try
        {
            manager.initialize(context);
        }
        catch (const std::logic_error&)
        {
            restart_rejected = true;
        }
        require(restart_rejected,"a faulted manager must reject reinitialization");

        mode = "normal";
        elysia::scene::SceneManager fresh_manager;
        fresh_manager.initialize(context);
        fresh_manager.register_game_scene<ProbeScene>(1);
        fresh_manager.start({.target = 1});
        require(fresh_manager.shutdown(),"a fresh manager must start after an independent cleanup failure");
        return 0;
    }
    Module module;
    if (mode == "init_backend")
    {
        require(!ELYSIA_INITIALIZE_APP(argc,argv,module),"initial scene backend failure must reject initialization");
        const auto info = elysia::tools::TerminationManager::instance()->termination_info();
        require(info && info->reason==elysia::tools::TerminationReason::FatalRuntimeFailure
            && info->category=="render" && info->message.find("injected initial scene backend failure")!=std::string_view::npos,
            "initial scene backend failure must preserve render diagnostic and fatal exit reason");
        require(SDL_WasInit(0)==0 && destroyed==1 && exits==0,
            "initial backend failure must close SDL and destroy the failed candidate without a second exit");
        return 0;
    }
    require(ELYSIA_INITIALIZE_APP(argc,argv,module),"GPU application initialization");
    int count = 0;
    auto windows = SDL_GetWindows(&count);
    require(count == 1,"one application window");
    auto renderer = SDL_GetRenderer(windows[0]);
    const bool vsync = argc > 3 && std::string(argv[3]) == "1";
    require(SDL_SetRenderVSync(renderer,vsync ? 1 : 0),"set probe VSync");
    if (mode != "timing") SDL_HideWindow(windows[0]);
    SDL_free(windows);
    const double fps = mode == "timing" && argc > 2 ? std::stod(argv[2]) : 0.01;
    require(ELYSIA_APP->apply_target_fps(fps).has_value(),"set probe fps");
    if (mode == "event")
    {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = SDLK_F2;
        require(SDL_PushEvent(&event),"open overlay");
        event.type = SDL_EVENT_USER;
        require(SDL_PushEvent(&event),"inject overlay event");
    }
    if (mode == "queue")
    {
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        event.user.code = 12345;
        require(SDL_PushEvent(&event),"queue ordinary input");
        int slices = 0;
        elysia::application::detail::wait_for_frame(0.01,[] { return 0.0; },
            [&] { SDL_PumpEvents(); return ++slices == 3; },[](auto,auto) {});
        require(SDL_PeepEvents(&event,1,SDL_GETEVENT,SDL_EVENT_USER,SDL_EVENT_USER) == 1
            && event.user.code == 12345,"waiting must preserve queued events");
    }
    std::atomic<std::uint64_t> requested_at = 0;
    std::jthread quitter;
    if (mode == "low" || mode == "fault" || mode == "queue")
    {
        quitter = std::jthread([&](std::stop_token stop)
        {
            while (!updated && !stop.stop_requested()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (stop.stop_requested()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            requested_at = SDL_GetPerformanceCounter();
            if (mode == "fault")
                elysia::tools::TerminationManager::instance()->request_termination(
                    elysia::tools::TerminationReason::UnhandledException,"test","injected asynchronous fault");
            else
            {
                SDL_Event event{};
                event.type = SDL_EVENT_QUIT;
                require(SDL_PushEvent(&event),"inject quit during wait");
            }
        });
    }
    std::vector<std::string> render_operations;
    static std::vector<std::string>* operation_log = nullptr;
    operation_log = &render_operations;
    if (mode.starts_with("render_") || mode=="cleanup_backend")
    {
        elysia::core::detail::render_operation_probe = [](std::string_view operation)
        {
            operation_log->emplace_back(operation);
            const bool fails = (mode=="render_clear" && operation=="SDL_RenderClear")
                || (mode=="render_draw" && operation=="SDL_RenderFillRect")
                || (mode=="render_present" && operation=="SDL_RenderPresent");
            if (fails) SDL_SetError("injected application render failure");
            return !fails;
        };
    }
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    auto result = ELYSIA_RUN_APP;
    elysia::core::detail::render_operation_probe = nullptr;
    const double latency_ms = requested_at ? (SDL_GetPerformanceCounter() - requested_at.load()) * 1000.0 / frequency : 0.0;
    require(exits == 1 && destroyed == 1,"application must exit and destroy the scene");
    require(SDL_WasInit(0) == 0,"SDL must be shut down after every exit path");
    const bool fault = mode.starts_with("exit_") || mode == "event" || mode == "fault" || mode.starts_with("render_") || mode=="cleanup_backend";
    require(result == (fault ? elysia::application::ApplicationRunResult::FaultExit
                            : elysia::application::ApplicationRunResult::NormalExit),"correct final exit result");
    if (mode=="cleanup_backend")
    {
        const auto info = elysia::tools::TerminationManager::instance()->termination_info();
        require(info && info->reason==elysia::tools::TerminationReason::FatalRuntimeFailure && info->category=="render",
            "backend error during scene recovery cleanup must produce FaultExit");
        require(info->message.find("backend detach after update failure")!=std::string_view::npos
            && info->message.find("primary update before backend cleanup")!=std::string_view::npos,
            "cleanup backend diagnostic and original ordinary failure must survive application shutdown");
        require(std::find(render_operations.begin(),render_operations.end(),"SDL_RenderPresent")==render_operations.end(),
            "backend recovery cleanup failure must skip frame presentation");
        require(frames.size()==1,"recovery cleanup backend failure must skip frame waiting");
    }
    if (mode.starts_with("render_"))
    {
        const auto info = elysia::tools::TerminationManager::instance()->termination_info();
        require(info && info->reason==elysia::tools::TerminationReason::FatalRuntimeFailure,
            "render failures must use the fatal runtime path");
        require(frames.size()==1,"render failure must stop the first frame before waiting");
        require(mode=="render_present" || std::find(render_operations.begin(),render_operations.end(),"SDL_RenderPresent")==render_operations.end(),
            "clear or draw failure must skip presentation");
    }
    if (mode.starts_with("exit_"))
        require(!elysia::tools::TerminationManager::instance()->termination_requested(),
            "cleanup failure after normal exit must work even when termination is sealed");
    if (mode == "event") require(overlay_stops == 1 && frames.empty(),"event failure must stop before update and clean overlay");
    if (requested_at)
    {
        std::cout << "Exit including cleanup: " << latency_ms << " ms\n";
        require(latency_ms < 2000.0,"low fps must not hold exit for the 100 second frame budget");
    }
    if (mode == "timing")
    {
        frames.erase(frames.begin(),frames.begin()+10);
        std::sort(frames.begin(),frames.end());
        double sum = 0;
        for (double value : frames) sum += value;
        std::cout << "TIMING fps=" << fps << " vsync=" << vsync
            << " mean_ms=" << sum / frames.size() * 1000.0
            << " p50_ms=" << frames[frames.size()/2] * 1000.0
            << " p95_ms=" << frames[frames.size()*95/100] * 1000.0
            << " max_ms=" << frames.back() * 1000.0 << '\n';
    }
}
