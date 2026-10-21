# Engine Architecture and Module Design

This directory is for developers understanding, maintaining, or extending the engine. Topics explain module responsibilities, object ownership, and runtime flows in current source. For game-layer calling examples, start with the [User Guide](../user-guide/README.md).

## Scenes and Game Runtime

- [Scene Lifecycle](subsystems/scene-lifecycle.md): caching, entry, exit, reset, safe mutation phases, exception recovery, and application shutdown order.
- [Input and Control](subsystems/input/README.md): device state through UI and player routing to control commands in fixed steps.
- [Physics](subsystems/physics/README.md): Box2D integration, bodies and joints, collision events, spatial queries, and map collision.
- [Cameras](subsystems/camera.md): slots, following and cinematics, scene-owned permissions, and the final presentation view.

## UI, Resources, and Persistence

- [UI Architecture](subsystems/ui/README.md#element-tree): cooperation between the element tree, layout, input, window surfaces, and themes.
- [Resource Loading Pipeline](subsystems/resources/README.md): request assembly, loading order, and service responsibilities.
- [Runtime Configuration](subsystems/runtime-config.md): startup defaults, user settings, read-only game configuration, and commit boundaries.
- [Audio Architecture](subsystems/audio.md): resource ownership, scheduling, track backend, and fades; public calling rules are in the [audio reference](../reference/audio.md).
- [Save Service](subsystems/save.md): typed data, reliable writing, recovery, and failure diagnostics.

## Development Tools and Acceptance Records

- [Development Demos](subsystems/development-demos.md): boundaries between Demo Gallery, game-side demos, and engine capabilities.
- [Development Overlay](subsystems/development-overlay.md): optional Dear ImGui integration, input capture, and panel lifecycles.
- [Showcase Refactoring Record](subsystems/showcase-validation.md) and [Camera Showcase Record](subsystems/camera-showcase-validation.md): historical results for particular dates and environments, not current working-tree test results.
