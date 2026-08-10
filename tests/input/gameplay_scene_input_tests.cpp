#define SDL_MAIN_HANDLED

#include "engine/gameplay/scene/gameplay_scene.h"
#include "tests/support/test_assertions.h"

#include <iostream>
#include <memory>

using elysia::tests::require;

namespace
{
class Receiver final
    : public elysia::core::GameObject
    , public elysia::gameplay::GameplayInputFrameReceiver
    , public elysia::gameplay::GameplayInputEventReceiver
{
public:
    Receiver(int order, bool consume)
        : GameObject(elysia::core::DepthLayer::Item, order), _consume(consume) {}

    void on_gameplay_input_frame(const elysia::gameplay::GameplayInputFrame&) override
    {
        ++frame_count;
    }

    bool on_gameplay_input_event(const elysia::input::ActionInputEvent&) override
    {
        ++event_count;
        return _consume;
    }

    int frame_count = 0;
    int event_count = 0;

private:
    bool _consume = false;
};

class TestGameplayScene final : public elysia::gameplay::GameplayScene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        low = create_and_add_object<Receiver>(1, false);
        high = create_and_add_object<Receiver>(2, true);
    }
    void on_exit() override {}
    void reset() override {}
    void enable_gameplay(bool enabled) { set_gameplay_input_enabled(enabled); }

    Receiver* low = nullptr;
    Receiver* high = nullptr;
};

class BasicScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        receiver = create_and_add_object<Receiver>(1, false);
    }
    void on_exit() override {}
    void reset() override {}
    Receiver* receiver = nullptr;
};

elysia::input::RawInputFrame jump_frame()
{
    elysia::input::RawInputFrame raw;
    raw.state.set_pressed(elysia::input::RawInputControl::KeySpace, true);
    return raw;
}
}

int main()
{
    BasicScene basic;
    basic.on_enter({});
    basic.on_input(jump_frame(), {});
    require(basic.receiver->frame_count == 0 && basic.receiver->event_count == 0,
        "Base Scene must not discover or dispatch gameplay receivers");

    TestGameplayScene scene;
    scene.on_enter({});
    scene.on_input(jump_frame(), {});
    require(scene.high->frame_count == 1 && scene.low->frame_count == 1,
        "GameplayScene must dispatch frames to all eligible receivers");
    require(scene.high->event_count == 1 && scene.low->event_count == 0,
        "Higher-priority event receivers must run first and may consume events");

    scene.high->set_active(false);
    scene.on_input(elysia::input::RawInputFrame{}, {});
    require(scene.low->event_count == 1,
        "Inactive receivers must be skipped without blocking lower receivers");

    scene.pause();
    scene.low->set_receive_input_when_paused(true);
    scene.on_input(jump_frame(), {});
    require(scene.low->frame_count == 3 && scene.low->event_count == 2,
        "Paused scenes must dispatch only to opted-in gameplay receivers");
    scene.resume();

    scene.enable_gameplay(false);
    const int frames_before_disable = scene.low->frame_count;
    scene.on_input(jump_frame(), {});
    require(scene.low->frame_count == frames_before_disable,
        "Disabled GameplayScene input must not dispatch");

    scene.enable_gameplay(true);
    scene.low->destroy();
    scene.on_input(jump_frame(), {});
    require(scene.low->frame_count == frames_before_disable,
        "Destroyed gameplay receivers must be pruned before dispatch");

    std::cout << "gameplay scene input tests passed\n";
    return 0;
}
