# Game Object Queries

This guide finds active-scene objects by type, drawing layer, and position. Layers here mean `DepthLayer`, not a parent/child tree. Use object queries to select business targets; use physics queries for geometric collision or occlusion.

`ELYSIA_OBJECT_QUERY` queries `GameObject` in the currently bound active scene. It is neither a global save database nor a query over UI or other cached scenes. Type, layer, and position queries need no physics collider.

## Finding Nearby Active Objects

Place this helper in game code and use it during normal input/update. `origin` and radius use world units.

```cpp
#include "engine/object_query/game_object_query_service.h"

std::vector<elysia::core::GameObject*> nearby_items(
    elysia::core::Vector2 origin, float radius)
{
    elysia::object_query::GameObjectQueryService* query = ELYSIA_OBJECT_QUERY;
    // The global entry borrows the active scene; do not query without a binding.
    if (!query->is_available())
        return {};
    // Returned pointers remain scene-owned and do not extend lifetime.
    return query->find_objects_in_radius<elysia::core::GameObject>(
        origin, radius, elysia::core::DepthLayerMask{elysia::core::DepthLayer::Item},
        [](const elysia::core::GameObject& object)
        {
            return object.is_active();
        });
}
```

Marked objects are excluded. Filter visibility/activity in predicates as needed; a result does not imply onscreen visibility or interaction availability. Replace the template type with a concrete game subclass if desired.

## Choosing a Query

| Need | Interface |
| --- | --- |
| One matching object | `find_object<T>` |
| All matching objects | `find_objects<T>` |
| Nearest/farthest from a world position | `find_nearest_object<T>`, `find_farthest_object<T>` |
| Centers within a radius | `find_objects_in_radius<T>` |

Overloads accept layer masks and predicates. Combine layers with `DepthLayer::Item | DepthLayer::Character`. Single-object misses return null; multi-object misses return an empty container. Unavailable active runtime cannot provide valid scene results. Negative radius returns empty.

Nearest, farthest, and radius queries measure distance to `object.center()`, including the radius boundary. They do not intersect object outlines with a circle or consider physical occlusion. For rays, shape overlap, sweeps, or contact filtering, use [physics queries](physics.md).

## Borrowed Results and Safe Boundaries

Result pointers are scene-owned; keeping a vector does not extend lifetime. Later switching, reconstruction, or cleanup can invalidate them. A single result is a matching object encountered during traversal, not a stable actor ID or business priority.

Predicates receive read-only references. Do not add, remove, or destroy scene objects during evaluation, including through captured references. For batch changes, finish querying first, then process still-valid results in the same business phase. The queried scene container is not safe to mutate during traversal.

Synchronous `add_object` / `clear_scene_objects` inside visitors/predicates is rejected. Physics query `noexcept` callbacks must likewise remain read-only; exceptions are not a query exit mechanism. Return results first, then mutate in an allowed phase.

## References

- [Public interfaces and overloads](../../../../engine/object_query/game_object_query_service.h)
- [Object ownership](../core_concepts.md), [back to user guide](../README.md)
