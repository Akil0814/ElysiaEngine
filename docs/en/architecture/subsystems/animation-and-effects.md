# Animation and Effect Architecture

This page explains shared animation definitions, independent playback instances, effect dispatch and scene ownership. See the [animation and effects guide](../../user-guide/systems/animation-and-effects.md) for integration and [resource loading](resources/README.md) for content registration.

<a id="animation-service-architecture"></a>
## Animation Service Architecture

The registry stores shared definitions; creation returns animation instances with independent playback state.

```mermaid
flowchart TB
  Loader["GameContentLoader"]
  Resources["ResourceService / ResourceManager<br/>Own atlases and textures"]
  Manager["AnimationManager<br/>Own AnimationDefinition registry"]
  Service["AnimationService / ELYSIA_ANIMATIONS"]
  Caller["Game objects / AnimationEffectFactory"]
  Instance["Animation<br/>Frame index, timing, looping and callbacks"]
  Loader -->|find_atlas after atlas completion| Resources
  Loader -->|register_animation: borrow atlas| Manager
  Manager -.->|Definitions borrow atlas pointers| Resources
  Caller -->|find_definition / create_animation| Service
  Service -->|Delegate definition lookup| Manager
  Service -->|Create independent instance from definition| Instance
  Caller -->|Receive unique_ptr and advance instance| Instance
  Instance -.->|Borrow atlas without owning textures| Resources
```

find_definition returns a borrow; create_animation returns a caller-owned unique_ptr. Both definitions and instances borrow resource atlases, so old instances and definitions must stop being used before reload. Missing definitions yield nullptr without on-demand loading.

Source: [animation_service.cpp](../../../../engine/animation/animation_service.cpp) · [animation_manager.h](../../../../engine/animation/runtime/animation_manager.h) · [animation.h](../../../../engine/animation/animation.h)

<a id="effect-service-architecture"></a>
## Effect Service Architecture

The Manager runtime owns screen effects; spawned animation and floating-number effect objects are transferred to the active Scene.

```mermaid
flowchart TB
  Caller["Game code / UI"]
  Service["EffectService / ELYSIA_EFFECTS"]
  Loader["GameContentLoader"]
  Manager["EffectManager<br/>Definition registry and request dispatch"]
  Screen["ScreenEffectRuntime<br/>Own screen effects and handle state"]
  Factories["AnimationEffectFactory / FloatingNumberEffectFactory"]
  Dependencies["AnimationService / FontResolver<br/>Animation instances and number glyph cache dependencies"]
  Scene["Active Scene<br/>Own spawned AnimationEffect / FloatingNumberEffect"]
  Lifecycle["SceneManager / Scene<br/>Binding, updates, rendering and exit cleanup"]
  Caller -->|Request, stop, cancel and query state| Service
  Service -->|Delegate requests and screen-effect controls| Manager
  Loader -->|register_animation_effect| Manager
  Manager -->|Owns; dispatches screen requests| Screen
  Manager -->|Owns; creates scene-object effects| Factories
  Factories -->|Create animations / Borrow font resolution| Dependencies
  Manager -.->|Borrow active scene; transfer spawned object ownership| Scene
  Lifecycle -->|Bind active scene and coordinate screen effects| Manager
  Lifecycle -->|Advance object lifecycle| Scene
```

EffectManager borrows the active scene and rendering/font dependencies; unavailable runtimes or definitions return explicit failures. Screen effects do not survive scene exit. Spawned effects follow scene-object lifetimes; EffectService does not load resources.

Source: [effect_service.cpp](../../../../engine/effects/effect_service.cpp) · [effect_manager.h](../../../../engine/effects/runtime/effect_manager.h) · [effect_manager.cpp](../../../../engine/effects/runtime/effect_manager.cpp)

Related: [Service Architecture Overview](../services.md) · [Architecture Index](../README.md).
