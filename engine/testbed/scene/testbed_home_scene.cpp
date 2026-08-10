#include "testbed_home_scene.h"

#include "../testbed_scene_keys.h"
#include "../../input/raw_input_types.h"
#include "../../builtin/scenes/application_failure_scene_payload.h"
#include "../../elysia/elysia_scene_payload.h"
#include "../../ui/containers/ui_list_container.h"
#include "../../ui/widgets/label/ui_label.h"
#include "../../ui/widgets/ui_button.h"
#include "../../ui/window/ui_window.h"

#include <memory>
#include <stdexcept>

namespace elysia::testbed
{
namespace
{
bool is_valid_return_route(const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}

std::unique_ptr<elysia::ui::UiButton> make_button(const char* label)
{
    auto button = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{ 0,0,360,56 });
    button->set_text_content(elysia::ui::ui_raw_text(label));
    return button;
}
}

void TestbedHomeScene::on_input(
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

void TestbedHomeScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const TestbedScenePayload* testbed_payload =
        elysia::scene::try_scene_payload<TestbedScenePayload>(payload);
    if (!testbed_payload || !is_valid_return_route(testbed_payload->return_route))
    {
        throw std::logic_error(
            "TestbedHomeScene requires TestbedScenePayload with a valid return route.");
    }

    _return_route = testbed_payload->return_route;
    _paused = false;

    if (!_root_window || _root_window->is_destroyed())
        build_ui();

    _root_window->set_visible(true);
    _root_window->set_active(true);
    _root_window->focus_first_available_scope();
}

void TestbedHomeScene::on_exit()
{
    _paused = false;
    if (_root_window && !_root_window->is_destroyed())
    {
        _root_window->set_active(false);
        _root_window->set_visible(false);
    }
}

void TestbedHomeScene::reset()
{
    _paused = false;
    _return_route = {};
    destroy_ui();
}

void TestbedHomeScene::build_ui()
{
    _root_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{ 0,0,1280,720 },100);
    if (!_root_window)
        throw std::runtime_error("TestbedHomeScene could not create its UiWindow.");

    _root_window->set_on_cancel([this]() { return_to_caller(); });

    auto list = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 460,120,360,480 });
    list->set_item_spacing(18.0f);

    auto title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0,0,360,72 },0,
        elysia::ui::ui_raw_text("Engine Testbed"));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    title->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    list->add_back(std::move(title));

    auto ui_test = make_button("UI Test");
    ui_test->set_on_click([this]() {
        request_scene_switch(
            SceneKeys::UiTest,
            TestbedScenePayload{ .return_route = make_home_route() });
    });
    list->add_back(std::move(ui_test));

    auto engine_feature_test = make_button("Engine Feature Test");
    engine_feature_test->set_on_click([this]() {
        request_scene_switch(
            SceneKeys::EngineFeatureTest,
            TestbedScenePayload{ .return_route = make_home_route() });
    });
    list->add_back(std::move(engine_feature_test));

    auto elysia_scene = make_button("Elysia Scene (1111)");
    elysia_scene->set_on_click([this]() {
        request_scene_switch(
            elysia::scene::SceneKeys::ElysiaEasterEgg,
            elysia::realm::ElysiaScenePayload{
                .return_route = make_home_route()
            });
    });
    list->add_back(std::move(elysia_scene));

    auto application_failure =
        make_button("Application Failure (Terminates App)");
    application_failure->set_visual_role(
        elysia::ui::UiButtonVisualRole::Danger);
    application_failure->set_on_click([this]() {
        request_scene_switch(
            elysia::builtin::make_application_failure_route(
                elysia::builtin::ApplicationFailurePresentation::
                    RuntimeFatal,
                "testbed",
                "Injected runtime failure from the Engine Testbed."));
    });
    list->add_back(std::move(application_failure));

    elysia::ui::UiListContainer* list_ptr = list.get();
    _root_window->add_child(std::move(list));
    _root_window->register_focus_scope(*list_ptr);
}

void TestbedHomeScene::destroy_ui() noexcept
{
    if (_root_window)
        _root_window->destroy();
    _root_window = nullptr;
}

void TestbedHomeScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}

elysia::scene::SceneRoute TestbedHomeScene::make_home_route() const
{
    return elysia::scene::SceneRoute{
        .target = SceneKeys::Home,
        .payload = TestbedScenePayload{ .return_route = _return_route },
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}
}
