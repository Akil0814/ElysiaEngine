#pragma once

#include "../../scene/scene.h"
#include "testbed_scene_payload.h"

namespace elysia::ui
{
class UiWindow;
}

namespace elysia::testbed
{
class TestbedHomeScene final : public elysia::scene::Scene
{
public:
    void on_input(
        const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;

private:
    void build_ui();
    void destroy_ui() noexcept;
    void return_to_caller();
    [[nodiscard]] elysia::scene::SceneRoute make_home_route() const;

private:
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiWindow* _root_window = nullptr;
};
}
