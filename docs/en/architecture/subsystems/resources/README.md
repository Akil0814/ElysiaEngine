# Resource Loading Pipeline

This page explains request assembly, submission, and handoff to services. For declaring assets, see the [resource guide](../../../user-guide/content/resources.md); complete JSON contracts are in the [format reference](../../../reference/resources/README.md).

<a id="resource-service-architecture"></a>
## Overall Loading Chain

The loader prepares and commits assets; the service queries already registered resources.

```mermaid
flowchart TB
  Registry["ContentRegistry<br/>Manifest entry snapshot"]
  Pipeline["ContentManifestPipeline / ResourceRequestAssembler<br/>Parse, assemble, validate and preflight"]
  Loader["GameContentLoader<br/>Own ResourceLoadPlan and orchestrate loading"]
  Worker["SurfaceLoader / AtlasBuildPreparer<br/>Workers: CPU decoding and preparation"]
  Commit["Main-thread submission<br/>TextureLoader and atlas frame commits"]
  Game["Game code and other services"]
  Service["ResourceService / ELYSIA_RESOURCES<br/>Query loaded resources by key"]
  Manager["ResourceManager<br/>Central writes, queries and cleanup"]
  Texture["TextureManager<br/>Textures and coverage masks"]
  Atlas["AtlasManager<br/>Atlases and build state"]
  Font["FontManager<br/>Sized fonts"]
  Audio["AudioManager<br/>Sound and music assets"]
  Cleanup["clear_loaded_content<br/>Effects → Animations → Configuration → Resources"]
  Registry -->|Read entries| Pipeline
  Pipeline -->|Produce load plan| Loader
  Loader -->|Dispatch preparation jobs| Worker
  Worker -->|Hand off through result queues| Commit
  Commit -->|store_texture / commit_prepared_atlas_frame| Manager
  Loader -->|Begin atlas builds; later load fonts and audio| Manager
  Game -->|find_* / has_*| Service
  Service -->|Delegate queries; return borrowed pointers| Manager
  Manager -->|Owns| Texture
  Manager -->|Owns| Atlas
  Manager -->|Owns| Font
  Manager -->|Owns| Audio
  Atlas -.->|Depends on: store frame textures and masks| Texture
  Loader -->|Clear on reload or failure| Cleanup
  Cleanup -->|clear: release atlases before textures| Manager
```

All query pointers are borrowed. Reloads and failures clear project content; ResourceManager clears Atlas, Texture, Font, then Audio. BuiltinResources and BootstrapTextureCache independently own built-ins and preloads outside the project pools.

Source: [resource_service.cpp](../../../../../engine/resources/resource_service.cpp) · [resource_manager.cpp](../../../../../engine/resources/runtime/resource_manager.cpp) · [game_content_loader.cpp](../../../../../engine/loading/game_content_loader.cpp)

Startup configuration, preloads and localization entries are handled during bootstrap and application initialization; project content in the diagram uses the corresponding required and additional manifests. `additional` does not select loaders by module name. New project modules use the same schema and are traversed in deterministic module-name order. `ryougi_sample` has an empty namespace and declares only animation capability; see the [actual configuration chain](../../../reference/resources/entity-content.md#current-repository-example).

The generic loader assembles requests in these stages. An undeclared capability produces no module requests in its stage:

1. Core `Animation`/`Atlas`.
2. `Animation`/`Atlas` from every `additional` module.
3. Core and module EffectDefinition.
4. Core and module Texture.
5. Font, core Audio, and module Audio.

At runtime, `Atlas` completes first, all `Animation` registrations follow, and EffectDefinition registrations come last. An `Effect` therefore binds an existing `Animation`; it does not load images or create animations.

Request origins, keys, and conflict checks are defined in the [format reference](../../../reference/resources/README.md#origins-and-duplicate-keys). Configuration snapshot publication is described in [runtime configuration](../runtime-config.md).
