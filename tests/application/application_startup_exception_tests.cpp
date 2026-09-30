#define SDL_MAIN_HANDLED
#include "engine/application/application.h"
#include "engine/tools/termination_manager.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <exception>
#include <new>

namespace
{
thread_local long fail_after = -1;
}
void* operator new(std::size_t size)
{
    if (fail_after >= 0 && fail_after-- == 0)
    {
        fail_after = -1;
        throw std::bad_alloc{};
    }
    if (auto* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

class Module final : public elysia::application::IGameModule
{
public:
    Module(bool fail_after_descriptor,long index,bool throw_in_descriptor,bool throw_in_scenes)
        : fail_after_descriptor(fail_after_descriptor), index(index),
          throw_in_descriptor(throw_in_descriptor), throw_in_scenes(throw_in_scenes) {}
    elysia::application::ApplicationDescriptor descriptor() const override
    {
        if (throw_in_descriptor) throw std::bad_alloc{};
        elysia::application::ApplicationDescriptor descriptor;
        descriptor.initial_route.target = 1;
        if (fail_after_descriptor) fail_after = index;
        return descriptor;
    }
    void register_scenes(elysia::scene::SceneManager&) const override
    {
        if (throw_in_scenes) throw std::bad_alloc{};
    }
    std::unique_ptr<elysia::tools::IDevelopmentOverlay> create_development_overlay() const override
    {
        if (!fail_after_descriptor && !throw_in_scenes) fail_after = index;
        return {};
    }
private:
    bool fail_after_descriptor;
    long index;
    bool throw_in_descriptor;
    bool throw_in_scenes;
};

int main(int argc,char** argv)
{
    std::set_terminate([] {
        try { if (auto exception = std::current_exception()) std::rethrow_exception(exception); }
        catch (const std::exception& error) { std::fprintf(stderr,"terminate: %s\n",error.what()); }
        catch (...) { std::fputs("terminate: unknown\n",stderr); }
        std::_Exit(86);
    });
    SDL_SetHint("ELYSIA_SUPPRESS_ERROR_DIALOGS","1");
    const bool late = argc > 1 && std::strcmp(argv[1],"late") == 0;
    Module module((argc < 2 || std::strcmp(argv[1],"mid") != 0) && !late,
        argc > 2 ? std::strtol(argv[2],nullptr,10) : 0,
        argc > 1 && std::strcmp(argv[1],"descriptor") == 0,late);
    const bool initialized = elysia::application::Application::instance()->initialize(argc,argv,module);
    fail_after = -1;
    const auto info = elysia::tools::TerminationManager::instance()->termination_info();
    elysia::tests::require(!initialized && info
            && info->reason == elysia::tools::TerminationReason::UnhandledException
            && info->category == "startup" && SDL_WasInit(0) == 0,
        "unexpected startup allocation failure uses final boundary and releases runtime");
}
