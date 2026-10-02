#pragma once
#include "game/showcase/input/local_controls_view.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/containers/ui_list_container.h"
namespace example::scene
{
class LocalMultiplayerScene final : public elysia::gameplay::GameplayScene
{
public:
    LocalMultiplayerScene();

protected:
    void on_enter(const elysia::scene::ScenePayload &) override;
    void on_exit() override;
    void on_reset() override {}
    void on_before_update(double) override;
    void on_after_update(double) override;
    bool on_unassigned_input(const elysia::input::RawInputEvent &) override;
    void on_shortcuts(const elysia::input::RawInputFrame &,
                      const std::vector<elysia::input::RawInputEvent> &) override;

  private:
    elysia::scene::SceneRoute _return_route;
    void open_menu();
    void close_menu();
    void configure_keyboard(bool swapped);
    void bind_players();
    void restore_devices();
    bool _swapped = false;
    elysia::gameplay::ControllerHandle _first_controller, _second_controller;
    std::optional<elysia::input::PlayerInputConfiguration> _saved_devices;
    elysia::input::KeyboardPartitionId _wasd, _arrows;
    elysia::core::GameObject *_first = nullptr;
    elysia::core::GameObject *_second = nullptr;
    elysia::input::LocalPlayerId _second_player{};
    elysia::input::LocalPlayerId _assign_to{};
    elysia::ui::UiWindow *_window = nullptr;
    example::showcase::input::LocalControlsView _view;
};
} // namespace example::scene
