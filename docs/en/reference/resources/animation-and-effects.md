# Animation and Effect JSON

This document explains how configurations generate animation, atlas, and `effect` requests. Effects reference existing `animations`; one `effect` key does not automatically create images/`animations`.

`Animation` handles `Atlas`/`Animation` resources. EffectDefinition registers existing `animations` as definitions without separate images/animation creation. Core/modules share request plans/registries.

Runtime `EffectService` also supports solid/image screen `effects`. Images query loaded texture keys without new definitions. Independent `EffectManager` runtime uses logical viewport coordinates before/after UI. Instances belong to the current scene and clear on unbinding, never persisting across scenes. Content cleanup releases `effects` before borrowed textures.

Missing-dependency queries and stage termination are distinct events: find_atlas/find_definition Warn records the dependency; later `Animation`/EffectDefinition Error records failed registration. Do not merge/delete these as duplicate logs.

## Core Animation

`assets/configs/manifests/animations_manifest.json`:

```json
{
  "animations": [
    {
      "key": "test.animation",
      "path": "test/frame_group.png",
      "frame_count": 14,
      "fps": 10,
      "loop": true,
      "horizontal_strip": true
    }
  ]
}
```

Entries require valid dotted key, assets/textures-relative path, positive integer `frame_count`, positive `fps`, boolean loop. `horizontal_strip` is optional `false`.

- Directory frames: path is a directory with nonempty `frame_prefix`.
- Horizontal strips: path is an ordinary image and `frame_prefix` is forbidden.

## Entity Animation Capability

This is the current `ryougi_sample` animation capability. See [repository example](entity-content.md#current-repository-example) for complete entry; this fragment cannot replace a module root.

```json
{
  "animations": {
    "texture_root": "textures/examples/ryougi/{id}",
    "config_template": "configs/examples/ryougi/{id}/animation_info.json",
    "frame_prefix_template": "{id}_{animation}{segment_suffix}",
    "layouts": {
      "fighter": "configs/examples/ryougi/animation_layout.json"
    }
  }
}
```

`texture_root`/`config_template`/nonempty `layouts` are required. Entities choose `animation_layout` and supply counts/playback parameters. See [entity modules](entity-content.md) for templates/paths.

## Animation Layout and Configuration

Layouts describe paths relative to entity texture roots:

```json
{
  "animations": {
    "idle": { "path": "idle" },
    "attack_normal": { "segment_path": "animation/attack/normal/{segment}" }
  }
}
```

Entries have path or `segment_path` only. `{segment}` substitutes two-digit filesystem numbering; absent markers still append the two-digit directory.

Entity animation configuration accepts `defaults`/`animations` only:

```json
{
  "defaults": { "source_type": "frame_directory" },
  "animations": {
    "idle": { "frame_count": 7, "fps": 10, "loop": true },
    "attack_normal": {
      "segments": [
        { "frame_count": 7, "fps": 10, "loop": false },
        { "frame_count": 8, "fps": 10, "loop": false }
      ]
    }
  }
}
```

Required `source_type` is `frame_directory`/`horizontal_strip` for the whole configuration, without per-entry override. Ordinary `animations`/every segment require positive `frame_count`/`fps` and boolean loop. Logical names must exist in selected `layouts`.

## Frame Sources and Segments

Directory frames generate strictly from configuration, without PNG scanning:

```text
<source>/<prefix>_000.png
<source>/<prefix>_001.png
...
```

Missing expected frames fail; extra PNGs are unread. Strips use one left-to-right row of equal frames without margins/spacing:

```text
<texture_root>/<resolved layout path>/<animation>.png
frame_width = image_width / frame_count
source_rect = { index * frame_width, 0, frame_width, image_height }
```

Only one texture decodes. Width must divide by `frame_count`, with positive frame width/image height.

Runtime segment keys are unpadded; filesystem numbering uses two digits:

```text
segment index            0                         1
runtime key              RyougiShiki.attack_normal.0 RyougiShiki.attack_normal.1
layout directory         .../00                    .../01
segment suffix           _00                        _01
first frame              ..._00_000.png             ..._01_000.png
```

Segments range `0–99`; frames start at 0 with at least three digits in filenames.

## Effects

Core `effects_manifest.json` maps existing `Animation` keys to EffectDefinition:

```json
{
  "effects": [
    {
      "key": "effect.test",
      "animation_key": "test.animation",
      "default_width": 128,
      "default_height": 128,
      "default_angle_degrees": 0
    }
  ]
}
```

These next two examples are hypothetical and require project configurations/`animations`/assets. `ryougi_sample` currently declares no `effects` capability. Module `effects` supplies per-entity config paths only:

```json
{
  "effects": {
    "config_template": "configs/character/{id}/effect_info.json"
  }
}
```

`effect_info.json` describes logical mapping only:

```json
{
  "effects": {
    "slash_trail": {
      "animation": "attack_normal",
      "default_width": 0,
      "default_height": 0,
      "default_angle_degrees": 0
    }
  }
}
```

`Effect`/animation names may differ. Segmented references generate one definition per configured segment; missing targets fail. Width/height must both be zero/omitted or both positive. Zero uses natural frame size during playback.

A custom `character_effects` module with `effect` namespace can generate `RyougiShiki.effect.attack_normal.0` and template prefix `RyougiShiki_effects_attack_normal_00`. These are possible configured results, not default `ryougi_sample` keys; projects supply files/assets. `Animation` registration precedes definitions.

## Resource Key Conflicts

Core/all-module requests deduplicate separately for `Atlas`/`Animation`/`Effect`/Texture/Font/Sound/Music. Strings may repeat across registries. Same-registry conflicts fail before submission, with both relative paths, JSON pointers, scopes, modules, `capabilities`, entities, logical names, segments.
