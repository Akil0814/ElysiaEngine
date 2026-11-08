# Active-scene Object Query Architecture

This page explains active-scene access and borrowed query results. See the [object-query guide](../../user-guide/systems/object-query.md) for usage and the [collision query contract](physics/05-collision-algorithms.md#collision-query-service-architecture) for physics geometry queries.

<a id="object-query-service-architecture"></a>
## Object Query Service Architecture

The global entry visits the object collection supplied by the current Scene; service templates perform type, predicate and score selection.

```mermaid
flowchart TB
  Caller["Game code<br/>Type, layer, predicate and distance queries"]
  Service["GameObjectQueryService / ELYSIA_OBJECT_QUERY<br/>Template filtering and result selection"]
  Manager["GameObjectQueryManager<br/>Only stores a borrowed active runtime"]
  SceneManager["SceneManager<br/>Active-scene attach / detach"]
  Runtime["Scene / IGameObjectQueryRuntime<br/>Visit game objects owned by the scene by layer"]
  Results["Borrowed GameObject pointers<br/>Exclude destroyed objects"]
  Caller -->|find_* / is_available| Service
  Service -->|Delegate candidate visitation| Manager
  SceneManager -->|bind_active_runtime / unbind_active_runtime| Manager
  Manager -.->|Borrow sole active runtime and visit| Runtime
  Runtime -->|Filter and select through service template callbacks| Results
  Results -.->|Return borrows without extending object lifetime| Caller
```

Results are borrowed, provide neither a spatial index nor exact physics tests, and do not extend object lifetime. is_available reports whether a runtime is bound; queries without candidates yield empty results. Re-query after scene switches or destruction, and obey scene safe-mutation phases during visitation.

Source: [game_object_query_service.h](../../../../engine/object_query/game_object_query_service.h) · [game_object_query_manager.cpp](../../../../engine/object_query/runtime/game_object_query_manager.cpp) · [scene.h](../../../../engine/scene/scene.h)

Related: [Service Architecture Overview](../services.md) · [Architecture Index](../README.md).
