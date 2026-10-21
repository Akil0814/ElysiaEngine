# Resource Loading and Use

For developers preparing assets/manifests, this guide explains declaration-to-loading flow, configuration types, and queries. Examples follow the current format, not permission for arbitrary fields.

Application/content loading centrally loads resources. Games query loaded data through `ELYSIA_RESOURCES`, without initializing/closing managers. Complete [game initialization](../game-initialization.md), load through [startup loading](../builtin-scenes/startup-loading.md), then enter dependent scenes after success.

## Minimal Workflow

1. Declare resources in the relevant manifests and place files under required roots.
2. Ensure `assets/content_registry.json` references them. Restart after registry/startup/preload edits: the registry parses at startup and is not reread by loading.
3. Query keys after success and handle empty results. [Game objects](../scene/game-objects.md) draw textures; [audio service](../systems/audio.md) plays sound. Content manifest edits also do not hot reload; restarting is the most direct development check.

Place this helper in game code after successful loading; the caller decides whether a missing image means skip or error flow.

```cpp
#include "engine/resources/resource_service.h"
#include "engine/tools/logger.h"

SDL_Texture* find_icon()
{
    // Query loaded keys rather than loading files within render/update loops.
    SDL_Texture* texture = ELYSIA_RESOURCES->find_texture("ui.moon");
    if (!texture)
        ELYSIA_LOG_ERROR("resources", "Missing texture: ui.moon");
    return texture; // Borrowed; never call SDL_DestroyTexture.
}
```

| Query | Result and use |
| --- | --- |
| `find_texture(key)` | Nullable `SDL_Texture` pointer for commands |
| `find_atlas(key)` | Nullable const `Atlas` with frames |
| `has_font(key)` / `find_font(key)` | Check/borrow registered sized fonts; keys include size |
| `find_sound(key)` / `find_music(key)` | Nullable MIX_Audio; ordinary playback uses `AudioService` |

All are borrowed. Do not use old pointers after failure/cleanup. Scene switching need not clear resources, but restarting loading first clears old project content. Do not reload while objects still use old resources or assume failure preserves them.

## Preparing Content Entry Points

Place an ordinary `assets/.elysia_root` file at the project root and prepare `assets/content_registry.json`. This registry references actual project manifests. Keep all seven required entries and follow each format.

```json
{
  "bootstrap": {
    "app_config": "configs/global/app_config.json",
    "preload_manifest": "configs/manifests/preload_manifest.json"
  },
  "manifests": {
    "required": {
      "configs": "configs/manifests/config_manifest.json",
      "fonts": "configs/manifests/fonts_manifest.json",
      "audio": "configs/manifests/audio_manifest.json",
      "i18n": "configs/manifests/i18n_manifest.json",
      "textures": "configs/manifests/textures_manifest.json",
      "animations": "configs/manifests/animations_manifest.json",
      "effects": "configs/manifests/effects_manifest.json"
    },
    "additional": {}
  }
}
```

Startup parses the registry; later loading borrows that snapshot. Restart after entry changes. See [registry reference](../../reference/resources/bootstrap-and-registry.md) for path resolution, required fields, and preloading.

Without a startup logo, use `{"textures": []}` for preload. Otherwise place `assets/preload/logo.png`, use this manifest, and pass `project.logo` to Payload `project_logo`:

```json
{
  "textures": [{ "key": "project.logo", "file": "logo.png" }]
}
```

Preload textures belong to the startup cache and do not automatically enter project texture services. Every declared image is required; missing/undecodable files terminate that startup stage.

## Adding a First Project Texture

1. Place the image at `assets/textures/ui/moon.png`.
2. Add this entry to the manifest referenced by `required.textures`.
3. Restart, wait for successful loading, then call `find_icon()` above. Query `ui.moon`, not its file path.

```json
{
  "textures": {
    "ui.moon": { "path": "ui/moon.png" }
  }
}
```

Keys use dot-separated alphanumeric/underscore components, without duplicates in one registry. Declare other core types through [core manifests](../../reference/resources/core-resource-manifests.md): fonts under `assets/fonts/`, audio under `assets/audio/`, translations under `assets/i18n/<language>/`.

Font declarations do not specify size. After presentation chooses a project font source, font settings generate required sizes. Use [audio](../systems/audio.md) and [localization](../systems/localization.md) services; do not release queried resources.

## Organizing Entity Content Packs

When actors share a layout, add module-name → manifest-path entries in `manifests.additional`. Names classify modules without selecting loader types.

Prepare content in this order:

1. Module manifest specifies entity list, resource namespace, and capabilities.
2. Entity list declares IDs; animated entities select declared layouts.
3. Layout describes file locations; entity configuration describes frame counts, playback, or effect mappings.
4. Place files and use generated keys after loading, such as `RyougiShiki.idle`.

See [entity content format](../../reference/resources/entity-content.md) for corresponding JSON. Do not add display names/combat stats to entity entries; use game configuration. Effects require animation capability in the same module; see [animation/effects](../systems/animation-and-effects.md#resource-configuration).

On failure, inspect logged configuration/JSON locations, substituted paths, layout names, and keys. Duplicate errors report both declarations; never rely on load ordering to overwrite another resource.

## Loading Failure and Lifetime

Every project preload entry is required. Configuration, missing files, decode, or registration failure must prevent entry into dependent scenes. Optional logo lookup differs from required preload files; see [startup loading](../builtin-scenes/startup-loading.md). Preload textures are not queried through `ResourceService`; their borrowing ends when `StartupLoadingScene` exits.

`StartupLoadingScene` drives normal loading; do not reload repeatedly in object updates. Failed project loading cleans partially committed content. After success, ordinary loader reset clears temporary state only; a new start begins reloading.

Built-in/project resources are separate; project cleanup does not clear built-ins. Do not overwrite reserved built-in names or release borrowed fonts/audio/textures/atlases.

Common errors: paths as keys, missing font size suffixes, Sound keys used as Music, premature queries, and old pointers after reload. Identical strings may coexist across types; duplicate registration within one type fails.

## Related Guides

- [Animation/effect configuration and playback](../systems/animation-and-effects.md), [configuration](configuration.md), [localization](../systems/localization.md)
- [Resource service](../../../../engine/resources/resource_service.h), [optional architecture](../../architecture/subsystems/resources/README.md)
- [Back to user guide](../README.md)
