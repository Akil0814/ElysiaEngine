#pragma once

#include "../../../engine/scene/scene.h"
#include "demo_scene_payload.h"

namespace elysia::ui
{
class UiWindow;
class UiConfirmationDialog;
}

namespace example::scene
{
class DemoGalleryScene final : public elysia::scene::Scene
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
    void reset_failure_confirmation() noexcept;
    [[nodiscard]] elysia::scene::SceneRoute make_gallery_route() const;

private:
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiWindow* _root_window = nullptr;
    elysia::ui::UiConfirmationDialog* _failure_confirmation = nullptr;
};
}
