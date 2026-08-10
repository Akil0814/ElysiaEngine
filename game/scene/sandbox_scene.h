#pragma once

#include "../../engine/scene/scene.h"

namespace elysia::ui
{
class UiWindow;
}

namespace example::scene
{
class SandboxScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;

private:
    void build_ui();
    void return_to_main_menu();

    elysia::ui::UiWindow* _window = nullptr;
};
}
