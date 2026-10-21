# Resource Loading Pipeline

This page explains request assembly, submission, and handoff to services. For declaring assets, see the [resource guide](../../../user-guide/content/resources.md); complete JSON contracts are in the [format reference](../../../reference/resources/README.md).

## Overall Loading Chain

```text
assets/content_registry.json
  ├─ bootstrap.app_config
  ├─ bootstrap.preload_manifest ─────────────> Startup texture preload
  ├─ manifests.required
  │   ├─ configs ────────────────────────────> Shared ConfigSnapshot (published after content loads)
  │   ├─ animations ─────────────────────────> Core Atlas + Animation
  │   ├─ effects ────────────────────────────> Core EffectDefinition
  │   ├─ textures/fonts/audio ───────────────> Core resources
  │   ├─ i18n ───────────────────────────────> LocalizationManager
  └─ manifests.additional
      └─ ryougi_sample ──────────────────────> Sample Animation/Atlas
```

The diagram matches the current repository registry. `additional` does not select loaders by module name. New project modules use the same schema and are traversed in deterministic module-name order. `ryougi_sample` has an empty namespace and declares only animation capability; see the [actual configuration chain](../../../reference/resources/entity-content.md#current-repository-example).

The generic loader assembles requests in these stages. An undeclared capability produces no module requests in its stage:

1. Core `Animation`/`Atlas`.
2. `Animation`/`Atlas` from every `additional` module.
3. Core and module EffectDefinition.
4. Core and module Texture.
5. Font, core Audio, and module Audio.

At runtime, `Atlas` completes first, all `Animation` registrations follow, and EffectDefinition registrations come last. An `Effect` therefore binds an existing `Animation`; it does not load images or create animations.

Request origins, keys, and conflict checks are defined in the [format reference](../../../reference/resources/README.md#origins-and-duplicate-keys). Configuration snapshot publication is described in [runtime configuration](../runtime-config.md).
