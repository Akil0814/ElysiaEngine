#include "engine/elysia/elysia_animation_loader.h"
#include "tests/support/test_assertions.h"

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <cstdlib>
#include <type_traits>

namespace
{
using elysia::tests::require;

void test_loader_lifecycle_skeleton()
{
    using elysia::realm::ElysiaAnimationLoader;
    using elysia::realm::ElysiaAnimationLoaderState;

    static_assert(!std::is_copy_constructible_v<ElysiaAnimationLoader>);
    static_assert(!std::is_copy_assignable_v<ElysiaAnimationLoader>);
    static_assert(!std::is_move_constructible_v<ElysiaAnimationLoader>);
    static_assert(!std::is_move_assignable_v<ElysiaAnimationLoader>);

    ElysiaAnimationLoader loader;
    require(
        loader.state() == ElysiaAnimationLoaderState::Unloaded
            && !loader.is_loading()
            && !loader.is_ready()
            && !loader.has_failed(),
        "Elysia animation loader must begin unloaded and error-free");

    loader.start();
    require(
        loader.state() == ElysiaAnimationLoaderState::Loading
            && loader.is_loading()
            && !loader.is_ready()
            && !loader.has_failed(),
        "starting the loader skeleton must enter Loading");

    loader.update();
    require(
        loader.state() == ElysiaAnimationLoaderState::Loading,
        "the empty update hook must not advance the loader state");

    loader.unload();
    loader.unload();
    require(
        loader.state() == ElysiaAnimationLoaderState::Unloaded
            && !loader.is_loading(),
        "unload must be repeatable and restore the initial state");
}
}

int main()
{
    test_loader_lifecycle_skeleton();
    return EXIT_SUCCESS;
}
