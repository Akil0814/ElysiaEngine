#define SDL_MAIN_HANDLED

#include "engine/application/presentation/application_window_settings.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace
{
using elysia::tests::require;

struct Recorder
{
    std::vector<std::string> calls;
    int fullscreen_result = 0;

    elysia::application::detail::ApplicationWindowOperations operations()
    {
        return {
            .set_fullscreen = [this](Uint32 flags)
            {
                calls.push_back(
                    flags == 0 ? "leave_fullscreen" : "borderless_fullscreen");
                return fullscreen_result;
            },
            .set_size = [this](int width,int height)
            {
                calls.push_back(
                    "size:" + std::to_string(width)
                    + "x" + std::to_string(height));
            },
            .center = [this]() { calls.push_back("center"); },
            .error_message = []() { return "injected SDL failure"; }
        };
    }
};
}

int main()
{
    Recorder windowed;
    require(
        elysia::application::detail::apply_window_settings(
            {
                elysia::config::WindowMode::Windowed,
                { 1600,900 }
            },
            windowed.operations()).has_value()
        && windowed.calls == std::vector<std::string>{
            "leave_fullscreen","size:1600x900","center"
        },
        "Windowed mode must leave fullscreen before resizing and centering");

    Recorder borderless;
    require(
        elysia::application::detail::apply_window_settings(
            {
                elysia::config::WindowMode::BorderlessFullscreen,
                { 1280,720 }
            },
            borderless.operations()).has_value()
        && borderless.calls == std::vector<std::string>{
            "borderless_fullscreen"
        },
        "borderless fullscreen must not overwrite the saved windowed size");

    Recorder failure;
    failure.fullscreen_result = -1;
    const auto failed =
        elysia::application::detail::apply_window_settings(
            {
                elysia::config::WindowMode::Windowed,
                { 1280,720 }
            },
            failure.operations());
    require(!failed && failed.error() == "injected SDL failure"
        && failure.calls == std::vector<std::string>{ "leave_fullscreen" },
        "fullscreen transition failure must stop later operations and propagate");

    return EXIT_SUCCESS;
}
