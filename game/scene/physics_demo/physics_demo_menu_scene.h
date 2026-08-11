#pragma once

#include "../../../engine/scene/scene.h"

namespace elysia::ui { class UiWindow; }

namespace example::scene
{
class PhysicsDemoMenuScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;
    void on_input(const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;
private:
    void build_ui();
    void return_to_main_menu();
    elysia::ui::UiWindow* _window = nullptr;
};
}
