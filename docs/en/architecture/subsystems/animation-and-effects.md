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

Separate Manager runtimes own screen effects and CPU particle groups; spawned animation and floating-number effect objects are transferred to the active Scene.

```mermaid
flowchart TB
  Caller["Game code / UI"]
  Service["EffectService / ELYSIA_EFFECTS"]
  Loader["GameContentLoader"]
  Manager["EffectManager<br/>Definition registry and request dispatch"]
  Screen["ScreenEffectRuntime<br/>Own screen effects and handle state"]
  Particles["ParticleRuntime / ParticleEffect<br/>Groups, contiguous arrays and group handles"]
  Factories["AnimationEffectFactory / FloatingNumberEffectFactory"]
  Dependencies["AnimationService / FontResolver<br/>Animation instances and number glyph cache dependencies"]
  Scene["Active Scene<br/>Own spawned AnimationEffect / FloatingNumberEffect"]
  Lifecycle["SceneManager / Scene<br/>Binding, updates, rendering and exit cleanup"]
  Caller -->|Request, stop, cancel and query state| Service
  Service -->|Delegate requests and screen-effect controls| Manager
  Loader -->|register_animation_effect| Manager
  Manager -->|Owns; dispatches screen requests| Screen
  Manager -->|Owns; dispatches particle requests| Particles
  Manager -->|Owns; creates scene-object effects| Factories
  Factories -->|Create animations / Borrow font resolution| Dependencies
  Manager -.->|Borrow active scene; transfer spawned object ownership| Scene
  Lifecycle -->|Bind active scene and coordinate screen effects| Manager
  Lifecycle -->|Advance object lifecycle| Scene
```

EffectManager borrows the active scene and rendering/font dependencies; unavailable runtimes or definitions return explicit failures. Screen effects do not survive scene exit. Spawned effects follow scene-object lifetimes; EffectService does not load resources.

Source: [effect_service.cpp](../../../../engine/effects/effect_service.cpp) · [effect_manager.h](../../../../engine/effects/runtime/effect_manager.h) · [effect_manager.cpp](../../../../engine/effects/runtime/effect_manager.cpp)

## Particle Runtime

After textures load, GameContentLoader registers and validates manifest presets through EffectManager. Game code requests and controls instances through EffectService. New instances copy parameters and borrow loaded textures. ParticleRuntime manages slot/generation group handles. Each ParticleEffect owns contiguous particle data and preallocated rendering storage, without GameObject registration or per-particle callbacks.

SceneManager advances particles before scene objects using scaled scene delta, skipping creation frames and paused frames. Scene draws particle batches after ordinary objects in each world depth layer, including empty layers. Groups retain creation order and each submits one SDL_RenderGeometry call. Rendering derives vertex color and size from particle lifetime, applies camera projection, and restores borrowed texture blend state on success and failure.

Stopping ends emission; cancellation retires the instance immediately. Scene unbinding, fault recovery, shutdown, and content cleanup clear particles before borrowed textures are released. Game code retains responsibility for collisions, damage, and character lifetime, updating emission positions by value.

Related: [Service Architecture Overview](../services.md) · [Architecture Index](../README.md).
