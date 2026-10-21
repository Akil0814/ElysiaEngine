# Entity Content Modules

This document describes additional modules' entity declarations. Module names organize content without selecting loaders. All use one format and feed shared registries.

Each `manifests.additional` entry is an arbitrarily named entity pack processed by one loader. The repository configures only `ryougi_sample`; names are not a code allowlist. See [repository example](#current-repository-example).

## Module Manifest

This hypothetical character-`effect` module illustrates combined animation/`effect` declarations. `configs/character/`, `textures/character/`, etc. are not bundled assets; projects must provide them. Later Texture/Audio declarations and `FlyingDemon` are hypothetical too.

```json
{
  "entities": "configs/character/characters_manifest.json",
  "key_namespace": "effect",
  "capabilities": {
    "animations": {
      "texture_root": "textures/character/{id}",
      "config_template": "configs/character/{id}/effect_animation_info.json",
      "frame_prefix_template": "{id}_effects_{animation}{segment_suffix}",
      "layouts": {
        "fighter": "configs/character/layouts/character_effect_animation_layout.json"
      }
    },
    "effects": {
      "config_template": "configs/character/{id}/effect_info.json"
    }
  }
}
```

Root accepts only required `entities`, `key_namespace`, `capabilities`. Namespace may be empty and `capabilities` an empty object. Optional `capabilities` are exactly `animations`, `effects`, `textures`, `audio`; unknown fields/`capabilities` fail. Effects requires `animations` in the same module.

## Capability Declarations

Animations requires `texture_root`, `config_template`, nonempty `layouts`. `config_template` must contain `{id}`. Entity `source_type` `frame_directory` also requires `frame_prefix_template`; strips do not use it.

`frame_prefix_template` allows only `{id}`, `{animation}`, `{segment_suffix}`, requiring the first two and segment_suffix for segmented animation. It is a filename prefix without path separators or `..`.

```json
{
  "textures": {
    "texture_root": "textures/character/{id}",
    "layout": "configs/character/layouts/character_texture_layout.json"
  },
  "audio": {
    "audio_root": "audio/character/{id}",
    "layout": "configs/character/layouts/character_audio_layout.json"
  }
}
```

Texture/Audio require roots/`layouts`. Textures may target files/nonrecursive directories; direct files sort by path and extension-free stems form keys. Audio requires ordinary files and currently generates Sound only.

## `{id}` and Path Resolution

`texture_root`/`audio_root` replace all `{id}` occurrences, or append ID if absent. No other marker is allowed; resolved roots must exist.

All `config_template` values explicitly require `{id}` and resolve to ordinary files. `Animation` `layouts` and Texture/Audio `layout` resolve from `assets/`; resource paths inside `layouts` resolve from instantiated roots.

## Entity Manifest

```json
{
  "entities": [
    {
      "id": "FlyingDemon",
      "enabled": true,
      "animation_layout": "normal"
    }
  ]
}
```

| Field | Rule |
| --- | --- |
| `id` | Required unique valid key component; directory identifier, template parameter, first runtime-key component. Entities conventionally use PascalCase. |
| `enabled` | Optional `true`; `false` skips entity. |
| `animation_layout` | Required for `Animation`, matching declared `layouts` keys; if present, must be a valid component. |

No other fields. ID syntax remains `[A-Za-z0-9_]+`; PascalCase is convention, not extra syntax. Display names remain independent i18n `display_name_key` and do not change with ID.

## Resource Keys

All `capabilities` use one builder:

```text
base      = <entity id>[.<key_namespace>]
Animation = <base>.<animation>[.<segment index>]
Effect    = <base>.<effect>[.<segment index>]
Texture   = <base>.<texture logical name>[.<directory file stem>]
Sound     = <base>.<audio logical name>
```

Hypothetical examples follow. Module names do not enter keys; entity IDs/namespaces do:

```text
characters / namespace ""      -> RyougiShiki.idle
character_effects / "effect"   -> RyougiShiki.effect.attack_normal.0
```

Components match `[A-Za-z0-9_]+`; dots only join. Empty components, hyphens, spaces, non-ASCII, consecutive/leading/trailing dots are invalid. Segment keys never pad; two digits belong to filesystem directories/prefixes only.

## Current Repository Example

| Module | Namespace | Capabilities |
| --- | --- | --- |
| `ryougi_sample` | `""` | `animations` |

Follow actual loading inputs:

1. [content_registry.json](../../../../assets/content_registry.json) references the module in `manifests.additional`.
2. [content_manifest.json](../../../../assets/configs/examples/ryougi/content_manifest.json) declares empty namespace, `entities`, templates.
3. [entities_manifest.json](../../../../assets/configs/examples/ryougi/entities_manifest.json) enables `RyougiShiki` with `fighter` `layout`.
4. [animation_layout.json](../../../../assets/configs/examples/ryougi/animation_layout.json) supplies entity-root-relative paths; [animation_info.json](../../../../assets/configs/examples/ryougi/RyougiShiki/animation_info.json) frame counts/FPS/loop/segments.
5. Texture root [textures/examples/ryougi/RyougiShiki](../../../../assets/textures/examples/ryougi/RyougiShiki) generates e.g. `RyougiShiki.idle`.

`Animation`-only samples do not remove generic Texture/Audio/Effect support. Modules may share entity lists. A custom `effect`-namespace module loads its own `animations`/registers definitions, not another module's base `animations` or undeclared sample `effect` keys automatically.

## Common Failures

- Missing/duplicate/invalid IDs or unknown entity fields;
- Missing `{id}`, unknown template markers, nonexistent resolved paths;
- Effects without same-module `animations`;
- `animation_layout` absent from `layouts`;
- Invalid Texture/Audio `layouts` or directory file stems;
- Core/module duplicates in one registry, reporting complete first/duplicate `ResourceOrigin`.
