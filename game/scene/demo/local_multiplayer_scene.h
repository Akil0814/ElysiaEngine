#pragma once
#include "../../../engine/gameplay/scene/gameplay_scene.h"
#include "../../../engine/ui/window/ui_window.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/containers/ui_list_container.h"
namespace example::scene
{
class LocalMultiplayerScene final : public elysia::gameplay::GameplayScene
{
  public:
    void on_enter(const elysia::scene::ScenePayload &) override;
    void on_exit() override;
    void reset() override
    {
        reset_input_routing();
    }
    void on_update(double) override;

  protected:
    bool on_unassigned_input(const elysia::input::RawInputEvent &) override;
    void on_shortcuts(const elysia::input::RawInputFrame &,
                      const std::vector<elysia::input::RawInputEvent> &) override;

  private:
    elysia::scene::SceneRoute _return_route;
    void open_menu();
    void close_menu();
    elysia::core::GameObject *_first = nullptr;
    elysia::core::GameObject *_second = nullptr;
    elysia::input::LocalPlayerId _second_player{};
    elysia::input::LocalPlayerId _assign_to{};
    elysia::ui::UiWindow *_window = nullptr;
    elysia::ui::UiListContainer *_menu = nullptr;
    elysia::ui::UiLabel *_status = nullptr;
};
} // namespace example::scene
