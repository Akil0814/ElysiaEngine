#include "engine/gameplay/control/controller_service.h"
#include "game/showcase/shared/showcase_catalog.h"
#include "game/showcase/shared/showcase_gallery_scene.h"

#include "game/navigation/showcase_scene_keys.h"
#include "game/navigation/main_menu_scene.h"
#include "engine/builtin/scenes/application_failure_scene_payload.h"
#include "engine/elysia/elysia_realm.h"
#include "engine/input/raw_input_types.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/layout/ui_layout_types.h"
#include "engine/ui/text/ui_text_content.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/window/ui_window.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace example::scene
{
namespace
{
[[nodiscard]] bool is_valid_return_route(
    const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}

}

void ShowcaseGalleryScene::on_shortcuts(const elysia::input::RawInputFrame &input,
                                    const std::vector<elysia::input::RawInputEvent> &events)
{
    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.control == elysia::input::RawInputControl::KeyEscape
            && event.type
                == elysia::input::RawInputEventType::ControlPressed)
        {
            consume_input(event);
            if (_root_window && _failure_confirmation
                && _root_window->is_overlay_open(*_failure_confirmation))
            {
                _failure_confirmation->close();
            }
            else
            {
                return_to_caller();
            }
            return;
        }
    }
}

void ShowcaseGalleryScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    auto* controllers = elysia::gameplay::ControllerService::instance();
    if (!controllers->session_active()) (void)controllers->begin_session();
    const ShowcaseEnterPayload* demo_payload =
        elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if (!demo_payload || !is_valid_return_route(demo_payload->return_route))
    {
        throw std::logic_error(
            "ShowcaseGalleryScene requires ShowcaseEnterPayload with a valid return route.");
    }

    _return_route = demo_payload->return_route;
    _paused = false;

    if (!_root_window || _root_window->is_destroyed())
        build_ui();

    reset_failure_confirmation();
    if (_root_window)
    {
        _root_window->set_visible(true);
        _root_window->set_active(true);
        _root_window->focus_first_available_scope();
    }
}

void ShowcaseGalleryScene::on_exit()
{
    _paused = false;
    reset_failure_confirmation();
    if (_root_window && !_root_window->is_destroyed())
    {
        _root_window->set_active(false);
        _root_window->set_visible(false);
    }
}

void ShowcaseGalleryScene::on_reset()
{
    _paused = false;
    _return_route = {};
    destroy_ui();
}

void ShowcaseGalleryScene::build_ui()
{
    const float logical_width = static_cast<float>(
        std::max(0, runtime_context().logical_width()));
    const float logical_height = static_cast<float>(
        std::max(0, runtime_context().logical_height()));
    _root_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0.0f, 0.0f, logical_width, logical_height}, 100);
    if (!_root_window)
        throw std::runtime_error(
            "ShowcaseGalleryScene could not create its UiWindow.");

    _root_window->set_on_cancel([this]() { return_to_caller(); });

    _failure_confirmation =
        _root_window->create_child<elysia::ui::UiConfirmationDialog>(
            elysia::core::Rect{0.0f, 0.0f, 460.0f, 250.0f}, 10);
    if (_failure_confirmation)
    {
        _failure_confirmation->set_config(
            elysia::ui::UiConfirmationDialogConfig{
                .title = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.title"),
                .message = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.message"),
                .confirm = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.confirm"),
                .cancel = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.cancel"),
                .close = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.close"),
                .confirm_visual_role =
                    elysia::ui::UiButtonVisualRole::Danger});
        _failure_confirmation->set_on_confirm([this]() {
            request_scene_switch(
                elysia::builtin::make_application_failure_route(
                    elysia::builtin::ApplicationFailurePresentation::
                        RuntimeFatal,
                    "demo_gallery",
                    elysia::core::make_failure_diagnostic("Injected runtime failure from the Demo Gallery.")));
        });
        (void)_failure_confirmation->register_with_window(*_root_window);
    }

    _view.build(*_root_window,"showcase.gallery.title","showcase.gallery.description",[this]{return_to_caller();});
    for (const auto& entry:example::showcase::kShowcaseEntries) {
        _view.add_action(entry.title,[this,key=entry.key,reload=entry.reload] {
            request_scene_switch(key,ShowcaseEnterPayload{make_gallery_route()},reload);
        });
        auto summary=std::make_unique<elysia::ui::UiLabel>(elysia::core::Rect{0,0,_view.content().screen_rect().width(),26},0,elysia::ui::ui_text_key(entry.description));
        summary->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit);
        _view.content().add_back(std::move(summary));
    }
    _view.content().add_back(std::make_unique<elysia::ui::UiLabel>(elysia::core::Rect{0,0,700,32},0,elysia::ui::ui_text_key("showcase.auxiliary")));
    _view.add_action("demo_gallery.elysia_realm",[this] {
        request_scene_switch(elysia::scene::SceneKeys::ElysiaRealm,elysia::realm::ElysiaRealmPayload{.return_route=make_gallery_route()});
    });
    auto* failure=_view.add_action("demo_gallery.failure_test",[this]{if(_failure_confirmation)_failure_confirmation->open();});
    failure->set_visual_role(elysia::ui::UiButtonVisualRole::Danger);
    _root_window->focus_first_available_scope();
}

void ShowcaseGalleryScene::destroy_ui() noexcept
{
    _view.clear();
    if (_root_window)
        _root_window->destroy();
    _root_window = nullptr;
    _failure_confirmation = nullptr;
}

void ShowcaseGalleryScene::reset_failure_confirmation() noexcept
{
    if (_failure_confirmation && !_failure_confirmation->is_destroyed())
        _failure_confirmation->close();
}

void ShowcaseGalleryScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}

elysia::scene::SceneRoute ShowcaseGalleryScene::make_gallery_route() const
{
    return elysia::scene::SceneRoute{
        .target = example::scene_keys::ShowcaseGallery,
        .payload = ShowcaseEnterPayload{.return_route = _return_route},
        .reload_mode = elysia::scene::SceneReloadMode::Reuse};
}
}
