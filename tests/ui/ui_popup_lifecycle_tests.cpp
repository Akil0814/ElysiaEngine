#define SDL_MAIN_HANDLED

#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/composites/ui_tooltip.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
using elysia::tests::require;

void test_overlay_lifetime()
{
    elysia::ui::UiConfirmationDialog dialog(elysia::core::Rect{ 0,0,320,180 });
    elysia::ui::UiWindow unowned_window(elysia::core::Rect{ 0,0,640,480 });
    require(!dialog.register_with_window(unowned_window),"unowned dialogs must not register as overlays");

    auto nested_host = std::make_unique<elysia::ui::UiPanel>(elysia::core::Rect{ 0,0,640,480 });
    auto nested_dialog = std::make_unique<elysia::ui::UiConfirmationDialog>(elysia::core::Rect{ 0,0,320,180 });
    auto* nested_dialog_raw = nested_dialog.get();
    nested_host->add_child(std::move(nested_dialog));
    unowned_window.add_child(std::move(nested_host));
    require(!nested_dialog_raw->register_with_window(unowned_window),"nested dialogs must not register as overlays");

    elysia::ui::UiWindow window(elysia::core::Rect{ 0,0,640,480 });
    auto* owned = window.create_child<elysia::ui::UiConfirmationDialog>(
        elysia::core::Rect{ 0,0,320,180 });
    require(owned != nullptr,"owned dialog should be created");
    require(owned->register_with_window(window),"direct window children should register as overlays");
    owned->open();
    require(window.is_overlay_open(*owned),"registered direct child should open");
    owned->destroy();
    window.update(0.0);
}

void test_transient_popup_lifetime()
{
    elysia::ui::UiDropdown dropdown(elysia::core::Rect{ 0,0,200,40 });
    dropdown.set_options({ { elysia::ui::ui_raw_text("one") } });
    {
        elysia::ui::UiWindow window(elysia::core::Rect{ 0,0,640,480 });
        dropdown.register_with_window(window);
        dropdown.open();
        require(dropdown.is_open(),"registered dropdown should open");
    }
    require(!dropdown.is_open(),"window detach should close dropdown");
    dropdown.open();
    require(!dropdown.is_open(),"detached dropdown must not reopen");

    elysia::ui::UiWindow first_window(elysia::core::Rect{ 0,0,640,480 });
    elysia::ui::UiWindow second_window(elysia::core::Rect{ 0,0,640,480 });
    dropdown.register_with_window(first_window);
    dropdown.register_with_window(first_window);
    dropdown.register_with_window(second_window);
    dropdown.unregister_from_window();
    dropdown.open();
    require(!dropdown.is_open(),"explicitly unregistered dropdown must remain closed");

    {
        auto short_lived = std::make_unique<elysia::ui::UiDropdown>(
            elysia::core::Rect{ 0,0,200,40 });
        short_lived->set_options({ { elysia::ui::ui_raw_text("one") } });
        short_lived->register_with_window(first_window);
    }
    first_window.update(0.0);

    auto owned = std::make_unique<elysia::ui::UiDropdown>(elysia::core::Rect{ 0,0,200,40 });
    auto* owned_raw = owned.get();
    owned_raw->set_options({ { elysia::ui::ui_raw_text("one") } });
    owned_raw->register_with_window(first_window);
    first_window.add_child(std::move(owned));
    owned_raw->destroy();
    first_window.update(0.0);
}

void test_tooltip_lifetime()
{
    elysia::ui::UiTooltip tooltip;
    {
        elysia::ui::UiWindow window(elysia::core::Rect{ 0,0,640,480 });
        tooltip.register_with_window(window);
    }
    tooltip.open();
    require(!tooltip.is_open(),"tooltip without content remains closed after window detach");

    elysia::ui::UiWindow window(elysia::core::Rect{ 0,0,640,480 });
    {
        auto short_lived = std::make_unique<elysia::ui::UiTooltip>();
        short_lived->register_with_window(window);
    }
    window.update(0.0);
}
}

int main()
{
    test_overlay_lifetime();
    test_transient_popup_lifetime();
    test_tooltip_lifetime();
    std::cout << "ui popup lifecycle tests passed\n";
    return EXIT_SUCCESS;
}
