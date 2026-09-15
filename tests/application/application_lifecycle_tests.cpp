#include "engine/application/application.h"
#include "engine/application/lifecycle/frame_pacing.h"
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
class ProbeScene final : public elysia::scene::Scene
{
public:
    ~ProbeScene() override { ++destroyed; }
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void reset() override {}
    void on_exit() override
    {
        ++exits;
        if (mode == "exit_standard") throw std::runtime_error("injected exit failure");
        if (mode == "exit_unknown") throw 42;
    }
    void on_update(double delta) override
    {
        updated = true;
        frames.push_back(delta);
        if (mode.starts_with("exit_") || (mode == "timing" && frames.size() >= 190))
            request_quit();
    }
    void on_render(SDL_Renderer*) override {}
};
class ThrowingOverlay final : public elysia::tools::IDevelopmentOverlay
{
public:
    std::expected<void,std::string> initialize(SDL_Window&,SDL_Renderer&) override { return {}; }
    void process_event(const SDL_Event&) override { throw std::runtime_error("injected event failure"); }
    void begin_frame(double) override {}
    void render(SDL_Renderer&) override {}
    void shutdown() noexcept override { ++overlay_stops; }
    elysia::input::DevelopmentInputCapture captured_input() const noexcept override
    { return elysia::input::DevelopmentInputCapture::None; }
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
    { manager.register_game_scene<ProbeScene>(1); }
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
        elysia::scene::SceneManager manager;
        manager.register_game_scene<ProbeScene>(1);
        manager.start({.target = 1});
        require(!manager.shutdown() && !manager.shutdown(),"cleanup failure must be sticky");
        require(exits == 1 && destroyed == 1,"exit exactly once and destroy on failure");
        manager.register_game_scene<ProbeScene>(1);
        mode = "normal";
        manager.start({.target = 1});
        require(manager.shutdown(),"a new manager lifecycle must reset cleanup result");
        return 0;
    }
    Module module;
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
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    auto result = ELYSIA_RUN_APP;
    const double latency_ms = requested_at ? (SDL_GetPerformanceCounter() - requested_at.load()) * 1000.0 / frequency : 0.0;
    require(exits == 1 && destroyed == 1,"application must exit and destroy the scene");
    require(SDL_WasInit(0) == 0,"SDL must be shut down after every exit path");
    const bool fault = mode.starts_with("exit_") || mode == "event" || mode == "fault";
    require(result == (fault ? elysia::application::ApplicationRunResult::FaultExit
                            : elysia::application::ApplicationRunResult::NormalExit),"correct final exit result");
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
