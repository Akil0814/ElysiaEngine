#include "engine/application/application.h"
#include "game/application/example_game_module.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <thread>
#include <chrono>

int main(int argc,char** argv)
{
    using elysia::tests::require;
    example::application::GameModule module;
    require(ELYSIA_INITIALIZE_APP(argc,argv,module),"example application must initialize on the GPU renderer");
    int count=0;
    SDL_Window** windows=SDL_GetWindows(&count);
    require(count>0,"application must own a window");
    for (int i=0;i<count;++i)
    {
        require(SDL_GetGPURendererDevice(SDL_GetRenderer(windows[i])) != nullptr,"application must use the GPU renderer");
        SDL_HideWindow(windows[i]);
    }
    SDL_free(windows);
    std::jthread quit([]
    {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        SDL_Event event{};
        event.type=SDL_EVENT_QUIT;
        require(SDL_PushEvent(&event),"application smoke test must enqueue normal exit");
    });
    require(ELYSIA_RUN_APP == elysia::application::ApplicationRunResult::NormalExit,
        "example application must draw frames and shut down normally");
}
