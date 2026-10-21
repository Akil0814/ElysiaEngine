# Elysia Engine Input

The input subsystem supports keyboard partitions, an independent mouse, concurrent devices, player ownership, shared UI coordination, and fixed-step command delivery. The game explicitly creates controllers. Single-player games need a game session to bound their lifetime, but do not need a network session.

```text
SDL → InputSystem::snapshot()
    → SceneInputRouter: independent UI → consumption/capture → shortcuts → keyboard partitions and device routing
    → LocalPlayerController: game action mapping
    → ControllerManager: cache → actual fixed tick → ControlCommandReceiver

Custom Controller: produce intent on a fixed tick → the same Manager cache and delivery
```

- [Architecture and lifecycle](architecture.md): Service, Manager, context, and scope.
- [Directories and dependencies](architecture.md#directories-and-dependencies): ownership of base input, UI input, scene routing, engine controllers, and game control code.
- [Action mapping](../../../reference/input/action-mapping.md): states, events, mouse deltas, and map changes.
- [Controllers and commands](../../../reference/input/controllers.md): game API, custom sources, and actor contracts.
- [Scene integration](../../../user-guide/gameplay/scene.md): sessions, explicit binding, fixed steps, and examples.
- [Interface migration](migration.md): breaking API changes and result handling.
- [Testing and debugging](testing-and-debugging.md): automated coverage and hardware acceptance.

The current implementation replaces the old scene input control interfaces. It provides no compatibility aliases, forwarding from old entry points, or dual broadcasts. External projects must migrate. AI decision-making, network transport, personal focus trees, split-screen, and input configuration persistence are outside the current scope.
