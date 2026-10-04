#define SDL_MAIN_HANDLED
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/detail/scene_failure_boundary.h"
#include "engine/ui/input/contracts/ui_input_frame_receiver.h"
#include "engine/ui/input/contracts/ui_input_event_receiver.h"
#include "tests/support/input_snapshot_builder.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/test_assertions.h"

#include <functional>
#include <iostream>
#include <stdexcept>

using namespace elysia::input;
using namespace elysia::gameplay;
using elysia::tests::require;
using elysia::scene::SceneTestAccess;

namespace
{
const InputActionId Move{"safety.move"};

template<class T> concept HasLegacyReset = requires(T& value) { value.reset(); };
template<class T> concept HasKeyboardRequirementMutation = requires(T& value) {
    value.clear_keyboard_requirement(PrimaryLocalPlayer);
};
template<class T> concept HasCancelHandlerMutation = requires(T& value) {
    value.set_cancel_handler({});
};
static_assert(!HasLegacyReset<LocalPlayerRegistry>);
static_assert(!HasKeyboardRequirementMutation<LocalPlayerRegistry>);
static_assert(!HasCancelHandlerMutation<elysia::scene::SceneInputRouter>);

struct Actor final : elysia::core::GameObject, ControlCommandReceiver
{
    Actor() : GameObject(elysia::core::DepthLayer::Character) {}
    std::function<void()> command_callback, cancel_callback;
    int commands = 0, cancellations = 0;
    void on_control_command(const ControlCommand&, double) override
    {
        ++commands;
        if (command_callback) command_callback();
    }
    void on_control_cancelled(InputCancelReason) override
    {
        ++cancellations;
        if (cancel_callback) cancel_callback();
    }
};

struct Driver final : Controller
{
    std::function<void()> cancel_callback;
    void produce_intent(std::uint64_t, double) override
    {
        ActionInputResult intention;
        intention.frame.set(Move, InputActionValueType::Axis1D, {1, 0});
        submit(std::move(intention));
    }
    void cancelled(InputCancelReason) override
    {
        if (cancel_callback) cancel_callback();
    }
};

struct LocalDriver final : LocalPlayerController
{
    explicit LocalDriver(LocalPlayerId player) : LocalPlayerController(player, {}) {}
};

struct World final : GameplayScene
{
    explicit World(World** output) { *output = this; }
    RawInputFrame shortcuts;
    std::vector<RawInputEvent> shortcut_events;
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
    void on_shortcuts(const RawInputFrame& frame, const std::vector<RawInputEvent>& events) override
    {
        shortcuts = frame;
        shortcut_events = events;
    }
};

struct Fixture
{
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context{nullptr, registry, 1280, 720};
    elysia::scene::SceneManager manager;
    World* world = nullptr;
    ControllerService* service = ControllerService::instance();
    Fixture()
    {
        manager.initialize(context);
        require(service->begin_session().has_value(), "Begin safety test session");
        manager.register_game_scene<World>(998, &world);
        manager.start({.target = 998});
    }
    ControllerHandle driver(Actor& target, ControllerScope scope = ControllerScope::Scene)
    {
        auto created = service->create<Driver>({scope, world->control_context().token()});
        require(created.has_value(), "Create safety controller");
        require(service->bind_target(*created, world->control_context(), target).succeeded(), "Bind safety controller");
        return *created;
    }
    void tick() { SceneTestAccess::update(*world, 1.0 / 60.0); }
};

template<class Callable> void require_exception(Callable&& callable, const char* message)
{
    bool caught = false;
    try { std::forward<Callable>(callable)(); }
    catch (const std::exception&) { caught = true; }
    require(caught, message);
}

struct Listener : elysia::ui::UiElement, elysia::ui::UiInputFrameReceiver, elysia::ui::UiInputEventReceiver
{
    std::function<void()> frame_callback, event_callback, cancel_callback;
    void cancel_input_interaction() override
    {
        if (cancel_callback) cancel_callback();
    }
    void on_ui_input_frame(const elysia::ui::UiInputFrame&) override
    {
        if (frame_callback) frame_callback();
    }
    bool on_ui_input_event(const elysia::ui::UiInputEvent&) override
    {
        if (event_callback) event_callback();
        return false;
    }
};

// Deliberately reuse an object's address to check registration identity, not only pointer membership.
struct PooledListener final : Listener
{
    alignas(64) static inline unsigned char storage[4096];
    static inline bool allocated = false;
    static void* operator new(std::size_t size)
    {
        require(!allocated && size <= sizeof(storage), "Single reusable listener slot");
        allocated = true;
        return storage;
    }
    static void operator delete(void*) noexcept { allocated = false; }
};

void test_ui_receiver_mutation()
{
    Fixture fixture;
    auto* first = fixture.world->create_and_add_object<Listener>();
    auto* second = fixture.world->create_and_add_object<Listener>();
    int original_frames = 0, added_frames = 0;
    second->frame_callback = [&] { ++original_frames; };
    bool added = false;
    first->frame_callback = [&] {
        if (std::exchange(added, true)) return;
        for (int index = 0; index < 64; ++index)
            fixture.world->create_and_add_object<Listener>()->frame_callback = [&] { ++added_frames; };
    };
    SceneTestAccess::dispatch_ui_frame(*fixture.world, {});
    require(original_frames == 1 && added_frames == 0, "Frame additions start at the next dispatch");
    SceneTestAccess::dispatch_ui_frame(*fixture.world, {});
    require(original_frames == 2 && added_frames == 64, "Frame additions participate on the next dispatch");

    int original_events = 0, added_events = 0;
    second->event_callback = [&] { ++original_events; };
    added = false;
    first->event_callback = [&] {
        if (std::exchange(added, true)) return;
        for (int index = 0; index < 64; ++index)
            fixture.world->create_and_add_object<Listener>()->event_callback = [&] { ++added_events; };
    };
    const std::vector<elysia::ui::UiInputEvent> events(2);
    (void)SceneTestAccess::dispatch_ui_events(*fixture.world, events);
    require(original_events == 2 && added_events == 0, "Event batch uses one stable receiver snapshot");
    (void)SceneTestAccess::dispatch_ui_events(*fixture.world, events);
    require(original_events == 4 && added_events == 128, "New event receivers join the next batch");
}

void test_ui_receiver_address_reuse()
{
    Fixture fixture;
    auto* first = fixture.world->create_and_add_object<Listener>();
    auto* second = fixture.world->create_and_add_object<PooledListener>();
    int old_deliveries = 0, new_deliveries = 0;
    second->frame_callback = [&] { ++old_deliveries; };
    bool replaced = false;
    first->frame_callback = [&] {
        if (std::exchange(replaced, true)) return;
        second->destroy();
        SceneTestAccess::update(*fixture.world, 0);
        auto* replacement = fixture.world->create_and_add_object<PooledListener>();
        require(replacement == second, "Probe really reuses the retired receiver address");
        replacement->frame_callback = [&] { ++new_deliveries; };
    };
    SceneTestAccess::dispatch_ui_frame(*fixture.world, {});
    require(old_deliveries == 0 && new_deliveries == 0, "Retired snapshot cannot call a reused address");
    SceneTestAccess::dispatch_ui_frame(*fixture.world, {});
    require(new_deliveries == 1, "Replacement joins the next frame");
}

void test_deferred_commit_exceptions()
{
    Fixture fixture;
    auto* first = fixture.world->create_and_add_object<Actor>();
    auto* replacement = fixture.world->create_and_add_object<Actor>();
    auto* other = fixture.world->create_and_add_object<Actor>();
    const auto handle = fixture.driver(*first);
    const auto other_handle = fixture.driver(*other);
    auto* driver = fixture.service->get<Driver>(handle);
    std::optional<ControllerOperation> failed, completed;
    driver->cancel_callback = [] { throw std::runtime_error("deferred cancel fault"); };
    first->command_callback = [&] {
        failed = fixture.service->bind_target(handle, fixture.world->control_context(), *replacement);
        completed = fixture.service->unbind_target(other_handle);
        require(failed->pending() && completed->pending(), "Command callback operations queue");
    };
    require_exception([&] { fixture.tick(); }, "Deferred cancellation exception propagates without terminate");
    require(failed->error() == ControllerError::CallbackFailed && completed->succeeded(),
        "Failed commit reaches a terminal result and other queued commits finish");
    require(!fixture.service->describe(handle)->bound && !fixture.service->describe(other_handle)->bound,
        "Failed and completed detachments leave no old bindings");
    require(first->cancellations == 1 && replacement->cancellations == 0 && other->cancellations == 1,
        "Controller failure cannot suppress the old target's cancellation or notify an unbound new target");
    driver->cancel_callback = {};
}

void test_session_cancellation_failure()
{
    Fixture fixture;
    auto* first = fixture.world->create_and_add_object<Actor>();
    auto* second = fixture.world->create_and_add_object<Actor>();
    const auto first_handle = fixture.driver(*first);
    const auto second_handle = fixture.driver(*second);
    std::optional<ControllerError> restart_error;
    fixture.service->get<Driver>(first_handle)->cancel_callback = [&] {
        require(!fixture.service->get(second_handle), "All session handles invalid before cancellation callbacks");
        auto restart = fixture.service->begin_session();
        require(!restart, "Session restart is rejected while teardown is executing");
        restart_error = restart.error();
        throw std::runtime_error("session controller fault");
    };
    second->cancel_callback = [] { throw std::runtime_error("session target fault"); };
    require_exception([&] { fixture.service->end_session(); }, "Report session teardown failure after closing every entry");
    require(restart_error == ControllerError::SessionEnding && !fixture.service->session_active(),
        "Session closing status has an explicit contract");
    require(!fixture.service->get(first_handle) && !fixture.service->get(second_handle), "Failed end invalidates all handles");
    require(first->cancellations == 1 && second->cancellations == 1, "Every target receives cancellation despite other failures");
    fixture.tick();
    require(second->commands == 0, "Ended session cannot deliver residual commands");
    require(fixture.service->begin_session().has_value(), "A fully closed session can restart after reporting failure");
    second->cancel_callback = {};
}

void test_pause_and_context_failure()
{
    Fixture fixture;
    auto* first = fixture.world->create_and_add_object<Actor>();
    auto* second = fixture.world->create_and_add_object<Actor>();
    const auto first_handle = fixture.driver(*first);
    const auto second_handle = fixture.driver(*second, ControllerScope::Session);
    auto* driver = fixture.service->get<Driver>(first_handle);
    driver->cancel_callback = [] { throw std::runtime_error("pause/context cancel fault"); };
    require_exception([&] { fixture.world->pause(); }, "Pause reports cancellation failure");
    require(first->cancellations == 1 && second->cancellations == 1, "Pause continues notifying later controllers");
    fixture.world->resume();
    require_exception([&] { SceneTestAccess::detach(*fixture.world); }, "Detach reports cancellation failure");
    require(!fixture.service->describe(first_handle)->bound && !fixture.service->describe(second_handle)->bound,
        "All context targets detach after callback failure");
    require(first->cancellations == 2 && second->cancellations == 2, "Detach notifies every target");
    const auto previous_token = fixture.world->control_context().token();
    require_exception([&] {
        SceneTestAccess::exit(*fixture.world);
        SceneTestAccess::reset(*fixture.world);
    }, "Reset reports unbound controller cancellation failure");
    require(fixture.world->control_context().token().generation > previous_token.generation,
        "Failed reset still advances the context generation");
    require(!fixture.service->get(first_handle) && fixture.service->get(second_handle),
        "Reset retires scene controllers and preserves session controllers");
    SceneTestAccess::attach(*fixture.world);
    SceneTestAccess::enter(*fixture.world);
    require(fixture.service->bind_target(second_handle, fixture.world->control_context(), *second).succeeded(),
        "Context is reusable after failed reset");
}

void test_destructor_fallback()
{
    Fixture fixture;
    World* isolated_world = nullptr;
    auto isolated = std::make_unique<World>(&isolated_world);
    SceneTestAccess::attach(*isolated);
    auto* target = isolated->create_and_add_object<Actor>();
    const auto created = fixture.service->create<Driver>({ControllerScope::Scene, isolated->control_context().token()});
    require(created && fixture.service->bind_target(*created, isolated->control_context(), *target).succeeded(),
        "Bind isolated destructor test controller");
    const auto handle = *created;
    int notifications = 0;
    fixture.service->get<Driver>(handle)->cancel_callback = [&] {
        ++notifications;
        throw std::runtime_error("destructor must not invoke this callback");
    };
    isolated.reset();
    require(notifications == 0 && !fixture.service->get(handle), "Destructor fallback closes references without user callbacks");
}

void test_scene_failure_and_backend_priority()
{
    {
        Fixture fixture;
        auto* first = fixture.world->create_and_add_object<Actor>();
        auto* target = fixture.world->create_and_add_object<Actor>();
        const auto handle = fixture.driver(*first);
        std::optional<ControllerOperation> operation;
        fixture.service->get<Driver>(handle)->cancel_callback = [] { throw std::runtime_error("scene deferred fault"); };
        first->command_callback = [&] {
            operation = fixture.service->bind_target(handle, fixture.world->control_context(), *target);
        };
        fixture.manager.on_update(1.0 / 60.0);
        require(fixture.manager.state() == elysia::scene::SceneManagerState::Faulted &&
            operation && operation->error() == ControllerError::CallbackFailed,
            "Deferred callback exceptions reach the actual SceneManager failure boundary");
        fixture.service->get<Driver>(handle)->cancel_callback = {};
    }
    {
        Fixture fixture;
        auto* first = fixture.world->create_and_add_object<Actor>();
        auto* second = fixture.world->create_and_add_object<Actor>();
        const auto handle = fixture.driver(*first);
        (void)fixture.driver(*second);
        fixture.service->get<Driver>(handle)->cancel_callback = [] { throw std::runtime_error("primary controller failure"); };
        second->cancel_callback = [] {
            throw elysia::core::RenderBackendError({"controller-cancel-render",
                elysia::core::make_failure_diagnostic("backend cancellation failure")});
        };
        elysia::scene::detail::SceneFailureCollector failures;
        try { fixture.service->end_session(); }
        catch (...) { failures.capture(998, elysia::scene::SceneBoundary::Exit, "Session end test"); }
        bool backend_preserved = false;
        try { (void)failures.finish(); }
        catch (const elysia::core::RenderBackendError& error)
        {
            backend_preserved = error.failure().operation == "controller-cancel-render" &&
                std::ranges::any_of(error.failure().diagnostic.entries, [](const auto& entry) {
                    return entry.reason.find("primary controller failure") != std::string::npos;
                });
        }
        require(backend_preserved && first->cancellations == 1 && second->cancellations == 1,
            "Cleanup preserves secondary backend failure priority and the original failure evidence");
        second->cancel_callback = {};
    }
}

void test_router_cancellation_failure()
{
    Fixture fixture;
    auto* first = fixture.world->create_and_add_object<Actor>();
    auto* second = fixture.world->create_and_add_object<Actor>();
    const auto other_player = fixture.world->local_players().create_player();
    for (const auto& [player, actor] : {std::pair{PrimaryLocalPlayer, first}, std::pair{other_player, second}})
    {
        auto created = fixture.service->create<LocalDriver>(
            {ControllerScope::Scene, fixture.world->control_context().token()}, player);
        require(created.has_value(), "Create local safety controller");
        require(fixture.service->bind_target(*created, fixture.world->control_context(), *actor).succeeded(),
            "Bind local safety controller");
    }
    first->cancel_callback = [] { throw std::runtime_error("player cancellation failure"); };
    require_exception([&] { fixture.world->set_all_gameplay_input_blocked(true); },
        "Router propagates direct cancellation failure");
    require(first->cancellations == 1 && second->cancellations == 1,
        "Router cancels every player despite a failing first callback");
    first->cancel_callback = {};
    fixture.world->set_all_gameplay_input_blocked(false);
    const int first_before = first->cancellations, second_before = second->cancellations;
    auto* listener = fixture.world->create_and_add_object<Listener>();
    int reset_frames = 0;
    listener->frame_callback = [&] { ++reset_frames; };
    listener->cancel_callback = [] { throw std::runtime_error("UI cancellation failure"); };
    require_exception([&] { fixture.world->set_ui_interaction_mode(elysia::scene::UiInteractionMode::Navigation); },
        "UI cancellation failure propagates");
    require(reset_frames == 1, "UI reset frame is delivered after cancellation failure");
    InputSnapshot input;
    input.focus_lost = true;
    require_exception([&] { SceneTestAccess::input(*fixture.world, input); },
        "Focus loss propagates failing UI cancellation");
    require(first->cancellations == first_before + 1 && second->cancellations == second_before + 1,
        "Pending player cancellations flush even when UI routing throws");
    listener->cancel_callback = {};
}

void test_partition_allocation_and_shortcut_devices()
{
    LocalPlayerRegistry players;
    const auto second = players.create_player();
    auto next = players.configuration();
    next.partitions.at(next.bindings.at(PrimaryLocalPlayer).keyboard).keys.erase(RawInputControl::KeyRight);
    next.partitions[KeyboardPartitionId{2}] = {KeyboardPartitionId{2}, "Imported", {RawInputControl::KeyRight}};
    next.bindings.at(second).keyboard = KeyboardPartitionId{2};
    require(players.apply_configuration(std::move(next)).has_value(), "Apply imported keyboard partition");
    const auto created = players.create_partition("New", {RawInputControl::KeyLeft});
    require(created && created->value != 2 && players.configuration().partitions.at(KeyboardPartitionId{2}).name == "Imported",
        "New partition IDs cannot overwrite imported bound partitions");
    require(players.allows_key(second, RawInputControl::KeyRight), "Imported key ownership stays intact");

    Fixture fixture;
    elysia::tests::InputSnapshotBuilder input;
    fixture.world->set_shortcut_devices(InputCapture::Keyboard);
    fixture.manager.on_input(input.take());
    input.press(RawInputControl::MouseLeft, true);
    input.press(RawInputControl::KeyD, true);
    fixture.manager.on_input(input.take());
    require(!fixture.world->shortcuts.state.is_pressed(RawInputControl::MouseLeft) &&
        fixture.world->shortcuts.state.is_pressed(RawInputControl::KeyD), "Shortcut frame obeys device mask");
    require(fixture.world->shortcut_events.size() == 1 && fixture.world->shortcut_events.front().device == InputDevice::Keyboard,
        "Shortcut events obey the same device mask");
    fixture.world->set_shortcut_devices(InputCapture::None);
    fixture.manager.on_input(input.take());
    require(!fixture.world->shortcuts.state.is_pressed(RawInputControl::KeyD) && fixture.world->shortcut_events.empty(),
        "Empty shortcut device mask excludes held frame state");
}
} // namespace

int main()
{
    test_ui_receiver_mutation();
    test_ui_receiver_address_reuse();
    test_deferred_commit_exceptions();
    test_session_cancellation_failure();
    test_pause_and_context_failure();
    test_destructor_fallback();
    test_scene_failure_and_backend_priority();
    test_router_cancellation_failure();
    test_partition_allocation_and_shortcut_devices();
    std::cout << "input safety tests passed\n";
}
