#pragma once

#include "application_failure_scene_payload.h"
#include "../../scene/scene.h"

#include <string>

namespace elysia::ui
{
class UiConfirmationDialog;
class UiButton;
class UiWindow;
struct UiConfirmationDialogConfig;
}

namespace elysia::builtin
{
class ApplicationFailureScene final : public elysia::scene::Scene
{
    friend class ApplicationFailureSceneTestAccess;

public:
    ApplicationFailureScene() = default;
    ~ApplicationFailureScene() override = default;

    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;
    void on_update(double delta) override;

private:
    void apply_payload(const ApplicationFailureScenePayload& payload);
    void build_ui();
    void open_dialog();
    void sync_dialog_state();
    void confirm_exit();
    void destroy_ui() noexcept;
    [[nodiscard]] static elysia::ui::UiConfirmationDialogConfig
        make_dialog_config(ApplicationFailurePresentation presentation);

private:
    ApplicationFailurePresentation _presentation =
        ApplicationFailurePresentation::RuntimeFatal;
    std::string _category;
    std::string _diagnostic_message;
    elysia::ui::UiWindow* _window = nullptr;
    elysia::ui::UiConfirmationDialog* _dialog = nullptr;
    elysia::ui::UiButton* _reopen_button = nullptr;
};
}
