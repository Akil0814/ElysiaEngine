#define SDL_MAIN_HANDLED

#include "engine/object_query/game_object_query_service.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "tests/support/test_assertions.h"

#include <functional>
#include <vector>

namespace
{
using elysia::tests::require;

class QueryProbe final : public elysia::core::GameObject
{
public:
    QueryProbe(
        int id,
        int score,
        const elysia::core::Vector2& center,
        elysia::core::DepthLayer layer,
        int order = 0)
        : GameObject(layer, order), _id(id), _score(score)
    {
        set_center(center);
    }

    [[nodiscard]] int id() const noexcept { return _id; }
    [[nodiscard]] int score() const noexcept { return _score; }

private:
    int _id = 0;
    int _score = 0;
};

class OtherProbe final : public elysia::core::GameObject
{
public:
    OtherProbe()
        : GameObject(elysia::core::DepthLayer::Terrain)
    {
    }
};

class FirstQueryScene final : public elysia::scene::Scene
{
public:
    FirstQueryScene()
    {
        instance = this;
        ++constructions;

        first = create_and_add_object<QueryProbe>(
            1,
            10,
            elysia::core::Vector2{0.0f, 0.0f},
            elysia::core::DepthLayer::Background
        );
        create_and_add_object<OtherProbe>();
        inactive = create_and_add_object<QueryProbe>(
            2,
            5,
            elysia::core::Vector2{3.0f, 4.0f},
            elysia::core::DepthLayer::Item
        );
        invisible = create_and_add_object<QueryProbe>(
            3,
            5,
            elysia::core::Vector2{6.0f, 8.0f},
            elysia::core::DepthLayer::Character
        );
        destroyed = create_and_add_object<QueryProbe>(
            4,
            20,
            elysia::core::Vector2{10.0f, 0.0f},
            elysia::core::DepthLayer::Foreground
        );

        inactive->set_active(false);
        invisible->set_visible(false);
        destroyed->destroy();
    }

    ~FirstQueryScene() override
    {
        if (instance == this)
            instance = nullptr;
    }

    void on_enter(const elysia::scene::ScenePayload&) override
    {
        ++enters;
        available_during_enter = ELYSIA_OBJECT_QUERY->is_available();
    }

    void on_exit() override
    {
        ++exits;
        available_during_exit = ELYSIA_OBJECT_QUERY->is_available();
    }

    void reset() override
    {
        ++resets;
    }

    static inline FirstQueryScene* instance = nullptr;
    static inline int constructions = 0;
    static inline int enters = 0;
    static inline int exits = 0;
    static inline int resets = 0;
    static inline bool available_during_enter = false;
    static inline bool available_during_exit = true;

    QueryProbe* first = nullptr;
    QueryProbe* inactive = nullptr;
    QueryProbe* invisible = nullptr;
    QueryProbe* destroyed = nullptr;
};

class SecondQueryScene final : public elysia::scene::Scene
{
public:
    SecondQueryScene()
    {
        instance = this;
        ++constructions;
        generation = constructions;
        only = create_and_add_object<QueryProbe>(
            20,
            1,
            elysia::core::Vector2{20.0f, 20.0f},
            elysia::core::DepthLayer::Item
        );
    }

    ~SecondQueryScene() override
    {
        if (instance == this)
            instance = nullptr;
    }

    void on_enter(const elysia::scene::ScenePayload&) override
    {
        ++enters;
        available_during_enter = ELYSIA_OBJECT_QUERY->is_available();
    }

    void on_exit() override
    {
        ++exits;
        available_during_exit = ELYSIA_OBJECT_QUERY->is_available();
    }

    void reset() override
    {
        ++resets;
    }

    static inline SecondQueryScene* instance = nullptr;
    static inline int constructions = 0;
    static inline int enters = 0;
    static inline int exits = 0;
    static inline int resets = 0;
    static inline bool available_during_enter = false;
    static inline bool available_during_exit = true;

    int generation = 0;
    QueryProbe* only = nullptr;
};

void reset_scene_state()
{
    FirstQueryScene::instance = nullptr;
    FirstQueryScene::constructions = 0;
    FirstQueryScene::enters = 0;
    FirstQueryScene::exits = 0;
    FirstQueryScene::resets = 0;
    FirstQueryScene::available_during_enter = false;
    FirstQueryScene::available_during_exit = true;

    SecondQueryScene::instance = nullptr;
    SecondQueryScene::constructions = 0;
    SecondQueryScene::enters = 0;
    SecondQueryScene::exits = 0;
    SecondQueryScene::resets = 0;
    SecondQueryScene::available_during_enter = false;
    SecondQueryScene::available_during_exit = true;
}

void test_empty_results_without_active_runtime()
{
    require(!ELYSIA_OBJECT_QUERY->is_available(),
        "object queries must start without an active runtime");
    require(ELYSIA_OBJECT_QUERY->find_object<QueryProbe>() == nullptr,
        "single-object queries must return null without an active runtime");
    require(ELYSIA_OBJECT_QUERY->find_objects<QueryProbe>().empty(),
        "multi-object queries must return an empty vector without an active runtime");
}

void test_query_algorithms_and_filters()
{
    constexpr elysia::scene::SceneKey first_key = 301;
    reset_scene_state();

    elysia::scene::SceneManager scene_manager;
    scene_manager.register_game_scene<FirstQueryScene>(first_key);
    scene_manager.start({ .target = first_key });

    require(ELYSIA_OBJECT_QUERY->is_available(),
        "starting a scene must bind the object query runtime");
    require(FirstQueryScene::available_during_enter,
        "the object query runtime must be available during on_enter");

    int predicate_calls = 0;
    QueryProbe* found = ELYSIA_OBJECT_QUERY->find_object<QueryProbe>(
        [&](const QueryProbe& probe)
        {
            ++predicate_calls;
            return probe.id() == 2;
        }
    );
    require(found && found->id() == 2,
        "find_object must return the first object accepted by the predicate");
    require(predicate_calls == 2,
        "find_object must stop visiting typed candidates after the first match");

    const std::vector<QueryProbe*> all =
        ELYSIA_OBJECT_QUERY->find_objects<QueryProbe>();
    require(all.size() == 3,
        "find_objects must exclude destroyed objects only");
    require(all[0]->id() == 1 && all[1]->id() == 2 && all[2]->id() == 3,
        "find_objects must preserve scene traversal order");
    require(all[1] == FirstQueryScene::instance->inactive,
        "inactive objects must remain queryable by default");
    require(all[2] == FirstQueryScene::instance->invisible,
        "invisible objects must remain queryable by default");

    const std::vector<QueryProbe*> filtered =
        ELYSIA_OBJECT_QUERY->find_objects<QueryProbe>(
            [](const QueryProbe& probe)
            {
                return probe.id() >= 2;
            }
        );
    require(filtered.size() == 2 && filtered[0]->id() == 2 && filtered[1]->id() == 3,
        "find_objects must apply the external predicate to every typed candidate");

    QueryProbe* best = ELYSIA_OBJECT_QUERY->find_best_object<QueryProbe>(
        [](const QueryProbe&) { return true; },
        [](const QueryProbe& probe) { return probe.score(); },
        std::less<int>{}
    );
    require(best && best->id() == 2,
        "find_best_object must retain the first object when scores tie");

    QueryProbe* highest_score =
        ELYSIA_OBJECT_QUERY->find_best_object<QueryProbe>(
            [](const QueryProbe& probe) { return probe.score(); },
            std::greater<int>{}
        );
    require(highest_score && highest_score->id() == 1,
        "the unfiltered find_best_object overload must consider every live typed object");

    const elysia::core::Vector2 origin = elysia::core::Vector2::zero();
    QueryProbe* nearest = ELYSIA_OBJECT_QUERY->find_nearest_object<QueryProbe>(
        origin,
        [](const QueryProbe& probe) { return probe.id() > 1; }
    );
    require(nearest && nearest->id() == 2,
        "find_nearest_object must use object centers and honor predicates");

    QueryProbe* farthest =
        ELYSIA_OBJECT_QUERY->find_farthest_object<QueryProbe>(origin);
    require(farthest && farthest->id() == 3,
        "find_farthest_object must ignore destroyed candidates");

    const std::vector<QueryProbe*> in_radius =
        ELYSIA_OBJECT_QUERY->find_objects_in_radius<QueryProbe>(origin, 5.0f);
    require(in_radius.size() == 2
        && in_radius[0]->id() == 1
        && in_radius[1]->id() == 2,
        "radius queries must include candidates exactly on the radius boundary");
    const std::vector<QueryProbe*> filtered_radius =
        ELYSIA_OBJECT_QUERY->find_objects_in_radius<QueryProbe>(
            origin,
            10.0f,
            [](const QueryProbe& probe) { return probe.id() >= 2; }
        );
    require(filtered_radius.size() == 2
        && filtered_radius[0]->id() == 2
        && filtered_radius[1]->id() == 3,
        "radius queries must combine spatial and external predicate filtering");
    require(ELYSIA_OBJECT_QUERY
        ->find_objects_in_radius<QueryProbe>(origin, -1.0f)
        .empty(),
        "negative radius queries must return an empty vector");

    scene_manager.shutdown();
    require(!FirstQueryScene::available_during_exit,
        "the object query runtime must be unavailable during on_exit");
    require(!ELYSIA_OBJECT_QUERY->is_available(),
        "scene shutdown must unbind the object query runtime");
}

void test_depth_layer_masks()
{
    using elysia::core::DepthLayer;
    using elysia::core::DepthLayerMask;

    constexpr DepthLayerMask three_layers =
        DepthLayer::Background | DepthLayer::Item | DepthLayer::Character;
    static_assert(three_layers.contains(DepthLayer::Background));
    static_assert(three_layers.contains(DepthLayer::Item));
    static_assert(three_layers.contains(DepthLayer::Character));
    static_assert(!three_layers.contains(DepthLayer::Terrain));
    static_assert(!DepthLayerMask::all().contains(DepthLayer::Count));

    constexpr elysia::scene::SceneKey first_key = 306;
    reset_scene_state();

    elysia::scene::SceneManager scene_manager;
    scene_manager.register_game_scene<FirstQueryScene>(first_key);
    scene_manager.start({ .target = first_key });

    const std::vector<QueryProbe*> character =
        ELYSIA_OBJECT_QUERY->find_objects<QueryProbe>(DepthLayer::Character);
    require(character.size() == 1 && character[0]->id() == 3,
        "single-layer queries must only visit the selected layer and retain invisible objects");

    const std::vector<QueryProbe*> selected =
        ELYSIA_OBJECT_QUERY->find_objects<QueryProbe>(three_layers);
    require(selected.size() == 3
        && selected[0]->id() == 1
        && selected[1]->id() == 2
        && selected[2]->id() == 3,
        "multi-layer queries must preserve scene layer traversal order");

    require(ELYSIA_OBJECT_QUERY
            ->find_objects<QueryProbe>(DepthLayerMask::all()).size() == 3,
        "the all-layer mask must match the legacy full-scene query");
    require(ELYSIA_OBJECT_QUERY
            ->find_objects<QueryProbe>(DepthLayerMask::none()).empty(),
        "an empty layer mask must not visit any objects");
    require(ELYSIA_OBJECT_QUERY
            ->find_object<QueryProbe>(DepthLayerMask::none()) == nullptr,
        "single-object queries with an empty mask must return null");
    require(ELYSIA_OBJECT_QUERY
            ->find_objects<QueryProbe>(DepthLayer::Foreground).empty(),
        "layer-restricted queries must continue to exclude destroyed objects");

    QueryProbe* filtered = ELYSIA_OBJECT_QUERY->find_object<QueryProbe>(
        DepthLayer::Item | DepthLayer::Character,
        [](const QueryProbe& probe) { return probe.id() == 3; });
    require(filtered && filtered->id() == 3,
        "layer masks and predicates must be applied together");

    const elysia::core::Vector2 origin = elysia::core::Vector2::zero();
    QueryProbe* nearest = ELYSIA_OBJECT_QUERY->find_nearest_object<QueryProbe>(
        origin,
        DepthLayer::Item | DepthLayer::Character,
        [](const QueryProbe& probe) { return probe.id() >= 2; });
    require(nearest && nearest->id() == 2,
        "nearest queries must score only candidates from selected layers");

    QueryProbe* farthest = ELYSIA_OBJECT_QUERY->find_farthest_object<QueryProbe>(
        origin,DepthLayer::Background | DepthLayer::Item);
    require(farthest && farthest->id() == 2,
        "farthest queries must score only candidates from selected layers");

    QueryProbe* best = ELYSIA_OBJECT_QUERY->find_best_object<QueryProbe>(
        DepthLayer::Item | DepthLayer::Character,
        [](const QueryProbe& probe) { return probe.id() == 3; },
        [](const QueryProbe& probe) { return probe.score(); },
        std::less<int>{});
    require(best && best->id() == 3,
        "best-object queries must combine masks, predicates, and projections");

    const std::vector<QueryProbe*> in_radius =
        ELYSIA_OBJECT_QUERY->find_objects_in_radius<QueryProbe>(
            origin,
            5.0f,
            DepthLayer::Item | DepthLayer::Character,
            [](const QueryProbe& probe) { return probe.id() >= 2; });
    require(in_radius.size() == 1 && in_radius[0]->id() == 2,
        "radius queries must combine their boundary, layer mask, and predicate");

    scene_manager.shutdown();
}

void request_switch(
    elysia::scene::SceneManager& manager,
    elysia::scene::SceneKey target,
    elysia::scene::SceneReloadMode reload_mode)
{
    elysia::scene::SceneRequest request;
    request.type = elysia::scene::SceneRequestType::Switch;
    request.route.target = target;
    request.route.reload_mode = reload_mode;
    manager.on_scene_request(request);
    manager.on_update(0.0);
}

void test_scene_lifecycle_rebinding()
{
    constexpr elysia::scene::SceneKey first_key = 311;
    constexpr elysia::scene::SceneKey second_key = 312;
    reset_scene_state();

    elysia::scene::SceneManager scene_manager;
    scene_manager.register_game_scene<FirstQueryScene>(first_key);
    scene_manager.register_game_scene<SecondQueryScene>(second_key);
    scene_manager.start({ .target = first_key });

    request_switch(scene_manager, second_key, elysia::scene::SceneReloadMode::Reuse);
    require(!FirstQueryScene::available_during_exit,
        "switching scenes must unbind the old runtime before on_exit");
    require(SecondQueryScene::available_during_enter,
        "switching scenes must bind the new runtime before on_enter");
    QueryProbe* current = ELYSIA_OBJECT_QUERY->find_object<QueryProbe>();
    require(current && current->id() == 20,
        "queries after a switch must only visit the new active scene");

    request_switch(scene_manager, second_key, elysia::scene::SceneReloadMode::Reset);
    require(SecondQueryScene::resets == 1
        && SecondQueryScene::enters == 2
        && SecondQueryScene::exits == 1,
        "reset reloads must detach, reset, rebind, and enter the reused scene");
    require(ELYSIA_OBJECT_QUERY->is_available(),
        "reset reloads must leave the query runtime bound");

    request_switch(scene_manager, second_key, elysia::scene::SceneReloadMode::Recreate);
    require(SecondQueryScene::constructions == 2,
        "recreate reloads must construct a fresh query runtime");
    require(SecondQueryScene::instance && SecondQueryScene::instance->generation == 2,
        "recreate reloads must activate the newly constructed scene generation");
    current = ELYSIA_OBJECT_QUERY->find_object<QueryProbe>();
    require(current && current == SecondQueryScene::instance->only,
        "recreate reloads must bind queries to the replacement scene");

    scene_manager.shutdown();
    require(!SecondQueryScene::available_during_exit,
        "shutdown must unbind the active runtime before on_exit");
    require(!ELYSIA_OBJECT_QUERY->is_available(),
        "shutdown must leave no active object query runtime");
}
}

int main()
{
    test_empty_results_without_active_runtime();
    test_query_algorithms_and_filters();
    test_depth_layer_masks();
    test_scene_lifecycle_rebinding();
    return 0;
}
