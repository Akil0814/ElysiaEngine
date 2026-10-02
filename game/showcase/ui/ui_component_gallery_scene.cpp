#include "game/showcase/ui/ui_component_gallery_scene.h"

#include "engine/builtin/resources/builtin_resources.h"
#include "engine/input/raw_input_types.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/ui/composites/ui_tab_container.h"
#include "engine/ui/containers/ui_chrome_container.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/window/ui_window.h"

#include <memory>
#include <stdexcept>

namespace example::scene
{
namespace
{
using namespace elysia::ui;

UiLayoutChildOptions at(float x,float y,float width,float height)
{
    return UiLayoutChildOptions{
        ._anchor = UiLayoutAnchor::TopLeft,
        ._margin = UiLayoutMargin{ x,y,0.0f,0.0f },
        ._cross_align = UiLayoutAlign::Start,
        ._size_override = elysia::core::Vector2(width,height),
        ._use_custom_cross_align = false,
        ._fill_cross_axis = false,
        ._use_size_override = true
    };
}

bool is_valid_return_route(const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}
}

void UiComponentGalleryScene::on_shortcuts(const elysia::input::RawInputFrame &input,
                                           const std::vector<elysia::input::RawInputEvent> &events)
{
    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.control == elysia::input::RawInputControl::KeyEscape
            && event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            consume_input(event);
            return_to_caller();
            return;
        }
    }
}

void UiComponentGalleryScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const ShowcaseEnterPayload* demo_payload =
        elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if (!demo_payload || !is_valid_return_route(demo_payload->return_route))
    {
        throw std::logic_error(
            "UiComponentGalleryScene requires ShowcaseEnterPayload with a valid return route.");
    }

    if (!elysia::builtin::BuiltinResources::instance()->is_initialized())
    {
        throw std::logic_error(
            "UiComponentGalleryScene requires initialized BuiltinResources.");
    }

    _return_route = demo_payload->return_route;
    _paused = false;
    rebuild_ui();
}

void UiComponentGalleryScene::on_exit()
{
    _paused = false;
    clear_ui();
}

void UiComponentGalleryScene::on_reset()
{
    _paused = false;
    clear_ui();
    _return_route = {};
}

void UiComponentGalleryScene::add_gallery_tab(
    elysia::ui::UiTabContainer& tabs,
    const char* label_key,
    std::unique_ptr<elysia::ui::UiScrollContainer> page)
{
    const elysia::ui::UiTabAddResult result = tabs.add_tab(
        elysia::ui::ui_text_key(label_key),std::move(page));
    if (!result.added)
        throw std::logic_error("UiComponentGalleryScene could not add a gallery tab.");
}

void UiComponentGalleryScene::refresh_theme_preview_styles()
{
    if (!_root_window)
        return;
    auto overrides = _root_window->style_overrides();
    overrides.draw_background = true;
    overrides.draw_border = true;
    _root_window->set_style_overrides(overrides);
}

void UiComponentGalleryScene::set_status_key(const char* key)
{
    if (_status_label)
        _status_label->set_text_content(elysia::ui::ui_text_key(key));
}

void UiComponentGalleryScene::set_active_theme(elysia::ui::UiBuiltinTheme theme)
{
    _theme_preview.manager.set_theme(theme);
    refresh_theme_preview_styles();
    sync_theme_switch_button_roles();
    set_status_key("ui_component_gallery.status.interaction");
}

void UiComponentGalleryScene::sync_theme_switch_button_roles() noexcept
{
    static constexpr std::array<elysia::ui::UiBuiltinTheme,7> themes{
        elysia::ui::UiBuiltinTheme::BlueGlassMoon,
        elysia::ui::UiBuiltinTheme::ElysiaLight,
        elysia::ui::UiBuiltinTheme::ElysiaDark,
        elysia::ui::UiBuiltinTheme::EvangelionUnit00,
        elysia::ui::UiBuiltinTheme::EvangelionUnit01,
        elysia::ui::UiBuiltinTheme::EvangelionUnit02,
        elysia::ui::UiBuiltinTheme::QuietSlate
    };

    for (std::size_t index = 0; index < themes.size(); ++index)
    {
        if (_pages._theme_buttons[index])
        {
            _pages._theme_buttons[index]->set_visual_role(
                _theme_preview.manager.current_builtin_theme() == themes[index]
                    ? elysia::ui::UiButtonVisualRole::Primary
                    : elysia::ui::UiButtonVisualRole::Default);
        }
    }
}

void UiComponentGalleryScene::rebuild_ui()
{
    clear_ui();
    SDL_Texture* image_texture = elysia::builtin::BuiltinResources::instance()->find_texture(
        elysia::builtin::BuiltinTextureId::ElysiaDefault);
    if (!image_texture)
        throw std::logic_error("UiComponentGalleryScene requires engine.brand.elysia.default.");

    // UiWindow is the scene-owned UI root. It owns the gallery tree and acts as
    // the registration authority for focus scopes, overlays, popups, and tooltips.
    _root_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0,0,float(runtime_context().logical_width()),float(runtime_context().logical_height())},100);

    _pages._root_window = _root_window;
    _root_window->set_on_cancel([this]() { return_to_caller(); });
    _theme_preview.attach(*_root_window);
    refresh_theme_preview_styles();

    auto status = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0,0,780,30 },0,
        elysia::ui::ui_text_key("ui_component_gallery.status.ready"));
    status->set_visual_role(elysia::ui::UiLabelVisualRole::Subtitle);
    _status_label = status.get();
    _root_window->add_child(std::move(status),at(24,88,850,30));

    // UiTabContainer creates and owns its UiTabBar and UiTabView internally.
    // Only the selected page is visible and active, so hidden page animations and
    // input handlers remain paused until the user selects their tab.
    auto workbench = std::make_unique<elysia::ui::UiTabContainer>(
        elysia::core::Rect{ 0,0,1080,530 });
    elysia::ui::UiTabContainer* tabs = workbench.get();
    tabs->set_on_selection_changed([this](auto) { cancel_hud_demo(); });

    add_gallery_tab(*tabs,"ui_component_gallery.pages.overview",_pages.build_overview_page());
    add_gallery_tab(*tabs,"ui_component_gallery.pages.states",_pages.build_states_page(image_texture));
    add_gallery_tab(*tabs,"ui_component_gallery.pages.controls",_pages.build_controls_page());
    add_gallery_tab(*tabs,"ui_component_gallery.pages.media",_pages.build_media_page(image_texture));
    add_gallery_tab(*tabs,"ui_component_gallery.pages.containers",_pages.build_containers_page());
    add_gallery_tab(*tabs,"ui_component_gallery.pages.overlays",_pages.build_overlays_page());
    add_gallery_tab(*tabs,"ui_component_gallery.pages.appearance",_pages.build_appearance_page());
    add_gallery_tab(*tabs,"ui_component_gallery.typography.tab",_pages.build_typography_page());

    _root_window->add_child(std::move(workbench),at(24,126,1080,490));
    _root_window->register_focus_scope(*tabs);
    example::showcase::ShowcaseFrame::build_chrome(*_root_window,"showcase.ui.title","showcase.ui.description",[this]{return_to_caller();});
    _root_window->focus_first_available_scope();
    sync_theme_switch_button_roles();
}

void UiComponentGalleryScene::clear_ui()
{
    _hud_view.clear();
    _hud_state={};
    _rebuild_requested=false;
    _pages._root_window = nullptr;
    _theme_preview.clear();
    _pages._theme_buttons.fill(nullptr);
    _status_label = nullptr;
    if (_root_window)
    {
        _root_window->destroy();
        _root_window = nullptr;
    }
}

void UiComponentGalleryScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}
}
