#include "ui_component_gallery_scene.h"

#include "../../../engine/builtin/resources/builtin_asset_cache.h"
#include "../../../engine/builtin/resources/builtin_asset_keys.h"
#include "../../../engine/input/raw_input_types.h"
#include "../../../engine/scene/runtime/scene_runtime_context.h"
#include "../../../engine/ui/composites/ui_tab_container.h"
#include "../../../engine/ui/containers/ui_chrome_container.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/window/ui_window.h"

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

void UiComponentGalleryScene::on_input(
    const elysia::input::RawInputFrame& input,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.control == elysia::input::RawInputControl::KeyEscape
            && event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            return_to_caller();
            return;
        }
    }

    elysia::scene::Scene::on_input(input,events);
}

void UiComponentGalleryScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const DemoScenePayload* demo_payload =
        elysia::scene::try_scene_payload<DemoScenePayload>(payload);
    if (!demo_payload || !is_valid_return_route(demo_payload->return_route))
    {
        throw std::logic_error(
            "UiComponentGalleryScene requires DemoScenePayload with a valid return route.");
    }

    const auto* cache = runtime_context().builtin_asset_cache();
    if (!cache || !cache->is_initialized())
    {
        throw std::logic_error(
            "UiComponentGalleryScene requires an initialized BuiltinAssetCache.");
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

void UiComponentGalleryScene::reset()
{
    _paused = false;
    clear_ui();
    _return_route = {};
}

std::unique_ptr<elysia::ui::UiButton> UiComponentGalleryScene::make_button(
    const char* text_key) const
{
    // UiButton owns the copied text descriptor and invokes its callback only after
    // a complete confirm or primary-pointer press/release sequence.
    return std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{ 0,0,240,40 },
        elysia::ui::UiButtonConfig{
            .content = elysia::ui::ui_text_key(text_key)
        },
        0);
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::make_page_scroll(
    elysia::ui::UiListContainer*& content) const
{
    // UiScrollContainer owns one content subtree, clips it to the viewport, and
    // delegates focus navigation into the active child scope while scrolling.
    auto page = std::make_unique<elysia::ui::UiScrollContainer>(
        elysia::core::Rect{ 0,0,900,390 });
    page->set_scroll_axis(elysia::ui::UiScrollAxis::Vertical);
    page->set_scrollbar_visibility(elysia::ui::UiScrollBarVisibility::Auto);
    page->set_scroll_step(elysia::core::Vector2(0.0f,36.0f));

    // UiListContainer adopts every row and derives both layout order and focus
    // neighbors from the same vertical child sequence.
    auto list = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0,0,870,0 });
    list->set_padding(elysia::ui::UiLayoutPadding{ 12,12,12,12 });
    list->set_item_spacing(12.0f);
    list->set_cross_align(elysia::ui::UiLayoutAlign::Start);
    content = list.get();
    page->set_content(std::move(list));
    return page;
}

elysia::ui::UiListContainer* UiComponentGalleryScene::add_section(
    elysia::ui::UiListContainer& page,
    const char* title_key,
    const char* description_key) const
{
    // UiChromeContainer separates header slots from its body while retaining a
    // single delegated focus region for all controls placed inside those slots.
    auto chrome = std::make_unique<elysia::ui::UiChromeContainer>(
        elysia::core::Rect{ 0,0,840,0 });
    chrome->set_header_height(42.0f);

    // UiLabel resolves localized single-line content through its typography and
    // semantic visual roles without accepting focus or input.
    auto heading = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0,0,420,32 },0,
        elysia::ui::ui_text_key(title_key));
    heading->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    chrome->add_title_child(std::move(heading));

    auto body = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0,0,820,0 });
    body->set_padding(elysia::ui::UiLayoutPadding{ 12,8,12,8 });
    body->set_item_spacing(8.0f);
    body->set_cross_align(elysia::ui::UiLayoutAlign::Start);

    auto note = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0,0,760,28 },0,
        elysia::ui::ui_text_key(description_key));
    note->set_visual_role(elysia::ui::UiLabelVisualRole::Muted);
    body->add_back(std::move(note));

    elysia::ui::UiListContainer* body_ptr = body.get();
    chrome->set_body(std::move(body));
    page.add_back(std::move(chrome));
    return body_ptr;
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
    _theme_manager.set_theme(theme);
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
        if (_theme_buttons[index])
        {
            _theme_buttons[index]->set_visual_role(
                _theme_manager.current_builtin_theme() == themes[index]
                    ? elysia::ui::UiButtonVisualRole::Primary
                    : elysia::ui::UiButtonVisualRole::Default);
        }
    }
}

void UiComponentGalleryScene::rebuild_ui()
{
    clear_ui();
    const auto* cache = runtime_context().builtin_asset_cache();
    if (!cache)
        throw std::logic_error("UiComponentGalleryScene requires BuiltinAssetCache while building UI.");

    SDL_Texture* image_texture = cache->find_texture(
        elysia::builtin::asset_keys::ElysiaDefaultTexture);
    if (!image_texture)
        throw std::logic_error("UiComponentGalleryScene requires engine.brand.elysia.default.");

    // UiWindow is the scene-owned UI root. It owns the gallery tree and acts as
    // the registration authority for focus scopes, overlays, popups, and tooltips.
    _root_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{ 80,52,1120,616 },100);
    _root_window->set_padding(elysia::ui::UiLayoutPadding{ 16,16,16,16 });
    _root_window->set_on_cancel([this]() { return_to_caller(); });
    _theme_registrations.push_back(_theme_manager.register_root(*_root_window));
    refresh_theme_preview_styles();

    auto status = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0,0,780,30 },0,
        elysia::ui::ui_text_key("ui_component_gallery.status.ready"));
    status->set_visual_role(elysia::ui::UiLabelVisualRole::Subtitle);
    _status_label = status.get();
    _root_window->add_child(std::move(status),at(16,12,850,30));

    // UiTabContainer creates and owns its UiTabBar and UiTabView internally.
    // Only the selected page is visible and active, so hidden page animations and
    // input handlers remain paused until the user selects their tab.
    auto workbench = std::make_unique<elysia::ui::UiTabContainer>(
        elysia::core::Rect{ 0,0,1080,530 });
    elysia::ui::UiTabContainer* tabs = workbench.get();

    add_gallery_tab(*tabs,"ui_component_gallery.pages.overview",build_overview_page());
    add_gallery_tab(*tabs,"ui_component_gallery.pages.states",build_states_page(image_texture));
    add_gallery_tab(*tabs,"ui_component_gallery.pages.controls",build_controls_page());
    add_gallery_tab(*tabs,"ui_component_gallery.pages.media",build_media_page(image_texture));
    add_gallery_tab(*tabs,"ui_component_gallery.pages.containers",build_containers_page());
    add_gallery_tab(*tabs,"ui_component_gallery.pages.overlays",build_overlays_page());
    add_gallery_tab(*tabs,"ui_component_gallery.pages.appearance",build_appearance_page());
    add_gallery_tab(*tabs,"ui_component_gallery.typography.tab",build_typography_page());

    _root_window->add_child(std::move(workbench),at(16,48,1080,530));
    _root_window->register_focus_scope(*tabs);
    _root_window->focus_first_available_scope();
    sync_theme_switch_button_roles();
}

void UiComponentGalleryScene::clear_ui()
{
    _theme_registrations.clear();
    _theme_buttons.fill(nullptr);
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
