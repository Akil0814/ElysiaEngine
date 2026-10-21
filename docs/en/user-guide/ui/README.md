# UI Guide

This guide connects windows, layout, text, themes, and focus through a quit menu with confirmation. Scenes/containers own elements, and standard scenes handle input, updates, and drawing. Games assemble interfaces and business callbacks.

Follow the integration steps below, then register the complete scene through [game initialization](../game-initialization.md). See [UI reference](../../reference/ui/README.md) for parameters/widget differences.

<a id="create-window"></a>
## Creating a Window

In `on_enter`, create a root with `create_and_add_object<UiWindow>`. It belongs to UI, separately managed from world `GameObject`. Coordinates use UI space; higher sibling `order` draws later and receives input earlier.

Scenes already convert raw input, update, and execute commands. Do not repeat routing/drawing in derived scenes. Parent layout assigns `screen_rect()`; presentation translation also requires coordinate conversion for hits, already handled by ordinary controls.

<a id="layout"></a>
## Content and Layout

The example window owns a vertical `UiListContainer`, which owns the quit button. `create_child` creates/transfers ownership; `add_child` receives `unique_ptr`. Returned pointers are borrowed, not caller-released. Move children through `extract_child`, then transfer to the new container.

Lists suit menus/action rows, grids regular icons, panels free composition. Configure anchors/margins/size through `UiLayoutChildOptions`, including existing fields `._anchor` / `._size_override`. Do not fight parent layout by overwriting child rects each frame.

For scrolling, group items in a list/grid, then let `UiScrollContainer::set_content` acquire that node. See [elements/layout](../../reference/ui/elements-and-layout.md) for measurement/container choices.

<a id="text-and-theme"></a>
## Text, Themes, and Local Styles

The example uses `ui_raw_text` for fixed text. Production projects may use `ui_text_key` with language translations; user input remains raw. `UiTextContent` expresses sources for names, messages, and button titles.

Keep `UiThemeManager` and the `UiThemeRegistration` returned by `register_root` as host members for future theme changes. Local registration objects detach at scope exit.

Primary describes a button's main-action role; local overrides fix only needed fields. This example fixes corner radius while colors follow themes. `set_style_overrides` replaces the whole override; clearing uses current base styles. See [styles/themes](../../reference/ui/style-and-theme.md).

<a id="focus"></a>
## Keyboard and Gamepad Focus

Register the list as a window focus scope and call `focus_first_available_scope()`. Lists manage internal navigation; `UiFocusScopeNeighbors` connects multiple scopes.

Ordinary `Scene` defaults to Navigation, so no repeated setup is needed. `GameplayScene` defaults Pointer; interactive menus switch to Navigation, then restore gameplay mode. UI gamepad choice is independent of player devices; see [menus/gameplay input](../gameplay/scene.md#menu-and-gameplay-input).

Temporary hiding/deactivation makes a scope unavailable without removing registration. Existing restoration can navigate after redisplay. Destruction/removal from the window subtree clears invalid registrations. Do not retain invalid `focused_target()` pointers.

<a id="popups"></a>
## Opening Confirmation Dialogs

Create `UiConfirmationDialog` as a direct window child and check `register_with_window`. Connect the button to `open()` only after registration. Confirmation closes first, then invokes business callbacks. Here it requests application exit; cancellation restores menu interaction.

Dialogs nested in panels fail registration. Dropdowns can register in nested window subtrees; tooltips are passive layers. See [windows/popups](../../reference/ui/windows-and-popups.md) for all three contracts.

<a id="cleanup"></a>
## Cleanup and Reuse

The example rebuilds on every entry and detaches themes before clearing objects on exit/reset. Window destruction releases its subtree; no individual button/dialog deletes are needed.

Buttons borrow dialogs and dialog callbacks borrow scenes, so their lifetimes must agree. Before independently removing a dialog, detach capturing button callbacks. `reset()` clears corresponding state; do not assume callbacks, selection, or subtree remain.

<a id="complete-example"></a>
## Complete Menu Scene

This complete type needs no showcase scene keys/translations. Failed dialog registration throws an integration exception handled by scene boundaries, rather than continuing with an unregistered overlay.

```cpp
#include "engine/scene/scene.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/style/ui_theme_manager.h"
#include <stdexcept>

class GuideMenuScene final : public elysia::scene::Scene
{
protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        clear_menu(); // Rebuild the menu on each entry in this example.
        elysia::ui::UiWindow* window = create_and_add_object<elysia::ui::UiWindow>(
            elysia::core::Rect{40, 40, 600, 360}, 100);
        window->set_padding({24.0f, 24.0f, 24.0f, 24.0f});
        elysia::ui::UiListContainer* actions =
            window->create_child<elysia::ui::UiListContainer>(
                elysia::ui::UiLayoutChildOptions{},
                elysia::core::Rect{0, 0, 220, 200});
        actions->set_direction(elysia::ui::UiListDirection::Vertical);
        actions->set_item_spacing(12.0f);

        elysia::ui::UiConfirmationDialog* dialog =
            window->create_child<elysia::ui::UiConfirmationDialog>(
                elysia::ui::UiLayoutChildOptions{});
        dialog->set_config({
            .title = elysia::ui::ui_raw_text("Exit game"),
            .message = elysia::ui::ui_raw_text("Return to desktop?"),
            .confirm = elysia::ui::ui_raw_text("Exit"),
            .cancel = elysia::ui::ui_raw_text("Cancel")});
        // The dialog must be a direct window child before overlay registration.
        if (!dialog->register_with_window(*window))
        {
            throw std::runtime_error("Failed to register exit dialog.");
        }
        dialog->set_on_confirm([this]
        {
            request_quit();
        });

        elysia::ui::UiButton* exit_button = actions->create_child<elysia::ui::UiButton>(
            elysia::ui::UiLayoutChildOptions{},
            elysia::core::Rect{0, 0, 180, 44},
            elysia::ui::UiButtonConfig{.content = elysia::ui::ui_raw_text("Exit")});
        // The callback borrows a dialog in the same window; this example destroys the whole menu tree together.
        exit_button->set_on_click([dialog]
        {
            dialog->open();
        });
        exit_button->set_visual_role(elysia::ui::UiButtonVisualRole::Primary);
        elysia::ui::UiButtonStyleOverrides overrides = exit_button->style_overrides();
        overrides.chrome.corner_radius = 8.0f;
        exit_button->set_style_overrides(overrides);

        window->register_focus_scope(*actions);
        window->focus_first_available_scope();
        _themes.set_theme(elysia::ui::UiBuiltinTheme::ElysiaDark);
        _theme_registration = _themes.register_root(*window);
    }

    void on_exit() override
    {
        clear_menu();
    }

    void on_reset() override
    {
        clear_menu();
    }

private:
    void clear_menu()
    {
        // Detach themes before cleaning the window and its subtree.
        _theme_registration.reset();
        clear_scene_objects();
    }

    elysia::ui::UiThemeManager _themes;
    elysia::ui::UiThemeRegistration _theme_registration;
};
```

## Continue by Task

| Task | Document |
| --- | --- |
| Elements, containers, fixed slots | [Elements/layout](../../reference/ui/elements-and-layout.md) |
| Widget APIs/results | [Browse by class](../../reference/ui/README.md#browse-by-class) |
| Actor names, world bars, bubbles | [World-space UI](../gameplay/world-ui.md) |
| Built-in settings | [Settings scene](../builtin-scenes/settings.md) |
| ImGui panels | [Development panels](../tools/development-panels.md) |
| Language/text sources | [Localization](../systems/localization.md) |
| UI implementation/extensions | [UI architecture](../../architecture/subsystems/ui/README.md#element-tree) |

[Back to user guide](../README.md)
