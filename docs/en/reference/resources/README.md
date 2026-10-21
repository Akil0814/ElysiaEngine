# Resource Configuration Formats

This directory records currently accepted JSON, paths, and resource keys. See [resource guide](../../user-guide/content/resources.md) for adding `assets` and [loading pipeline](../../architecture/subsystems/resources/README.md) for execution.

- [Bootstrap registry and preload](bootstrap-and-registry.md)
- [Core manifests](core-resource-manifests.md): fonts, audio, textures, internationalization.
- [Entity content modules](entity-content.md): capabilities, templates, layouts, keys.
- [Animation/effects](animation-and-effects.md): sources, segments, mappings.
- [Runtime configuration](../../architecture/subsystems/runtime-config.md): snapshot internals; see [configuration guide](../../user-guide/content/configuration.md) for games.

## Path Roots

| Configuration | Relative root |
| --- | --- |
| Registry, module manifests, layout `configs/...` | `assets/` |
| Core font file | `assets/fonts/` |
| Core audio path | `assets/audio/` |
| Core texture/animation path | `assets/textures/` |
| Preload textures | `assets/preload/` |
| i18n files | `assets/i18n/<language>/` |
| Module `Animation`/`Texture` layouts | Resolved entity `texture_root` |
| Module Audio layouts | Resolved entity `audio_root` |

`PathManager` preserves absolute paths. Relative paths beginning with `assets` resolve from project root; `configs` paths resolve from `assets/`.

## Resource Key Syntax

Explicit keys and components of derived keys share this rule:

```text
[A-Za-z0-9_]+
```

Dots join components only. Empty components, consecutive/leading/trailing dots, hyphens, spaces, non-ASCII, and other separators are invalid; numeric components are valid. Applies to entity IDs, nonempty `key_namespace`, layouts, logical animation/effect/texture/audio names, extension-free directory texture filenames, and dot-split `core` keys. Entity IDs conventionally use PascalCase. `key_namespace: ""` is the sole empty-key input allowed and is skipped in construction.

Module resources use:

```text
<entity id>[.<key_namespace>].<logical component>[.<segment index>]
```

Segment keys use unpadded decimals `.0`, `.1`, `.99`. Two-digit padding belongs only to filesystem paths/prefixes; see [animation/effects](animation-and-effects.md#frame-sources-and-segments).

## Origins and Duplicate Keys

Every request carries `ResourceOrigin` for diagnosis:

- Project-relative configuration path;
- JSON pointer or array index;
- Separate `core`/`additional` origin scope and module name (even `core`-named/empty modules cannot be confused with `core` origins);
- Capability declaration;
- Entity ID, logical name, segment/index where applicable.

Before submission, `core`/all-module requests merge for per-registry duplicate checks: `Atlas`, `Animation`, `Effect`, `Texture`, `Font`, `Sound`, `Music`. Duplicates terminate assembly and report complete `first`/duplicate origins. The same string across registries is valid.

## General Conventions

- JSON must be readable/valid; allowed unknown fields depend on each section.
- Several critical formats reject duplicate properties before parsing to avoid silent overwrite.
- Repeated same-named JSON members are schema errors before declarations generate. `first`/`second` `ResourceOrigin` diagnoses distinct schema-valid declarations resolving to one registry/key.
- Existence checks may occur during registry/module/request/preparation stages. Failure terminates that loading stage and logs.
- This directory does not describe business JSON internals for `app_config`, `input_config`, `game_config`, maps, levels, combat stats, skills, or user settings.
