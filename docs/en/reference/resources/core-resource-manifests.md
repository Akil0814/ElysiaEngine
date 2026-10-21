# Core Resource Manifests

This reference describes core font/audio/texture/localization fields. Keys determine registration identity; paths determine loading location. They are not interchangeable.

`content_registry.json` `manifests.required` references core resources. See [animation/effects](animation-and-effects.md) for `Animation`/`Effect`.

Explicit core keys split on dots; each component matches `[A-Za-z0-9_]+`. `ui.moon` is valid; `ui..moon`, `.ui`, `ui-moon`, and `界面.moon` are invalid.

## Fonts

```json
{
  "fonts": [
    { "key": "ui.latin", "file": "fusion-pixel.ttf" }
  ]
}
```

| Field | Type | Rule |
| --- | --- | --- |
| `fonts` | array<object> | Required, nonempty |
| `fonts[].key` | string | Required, nonempty valid dotted key |
| `fonts[].file` | string | Required, nonempty, relative to `assets/fonts/` |

Manifests describe project font-family files, not sizes. `Application` resolves sizes centrally; absent overrides use engine Typography Profile and 20pt floating numbers. Each entry generates Font requests for final sizes as `<font key>.<size>`, e.g. `ui.latin.30`. No selected project font source means no project requests. Derived numeric sizes also pass the shared key builder. File reading/font creation occurs at submission.

Array JSON pointers enter `ResourceOrigin`. Duplicate derived Font keys, even across entries, report both origins during request-plan Font checks.

## Audio

```json
{
  "sounds": {
    "system.confirm": { "path": "system/confirm.wav" }
  },
  "music": {
    "scene.main": { "path": "scene/main.ogg" }
  }
}
```

| Field | Type | Rule |
| --- | --- | --- |
| `sounds` | object | Required; property names become Sound keys |
| `music` | object | Required; property names become Music keys |
| Entry path | string | Required, relative to `assets/audio/` |

Both objects may be empty. Names must be valid dotted keys; entries accept path only. Duplicate JSON members reject; later audio loading reports reads/decoding.

Sound/Music registries are distinct, allowing identical strings. Conflicts with module Audio within one registry fail with both complete origins.

## Textures

```json
{
  "textures": {
    "ui.moon": { "path": "ui/moon.png" }
  }
}
```

| Field | Type | Rule |
| --- | --- | --- |
| `textures` | object | Required |
| Property name | string | Valid dotted Texture key |
| Entry path | string | Required, relative to `assets/textures/` |

Entries accept path only. Request generation requires ordinary files, not directories. Duplicate JSON properties reject.

Core/all-module `textures` share one duplicate registry. Errors include both configuration paths, JSON pointers, core/module, capability, entity, logical name.

## Internationalization Manifest

```json
{
  "default_language": "en",
  "languages": ["en", "ja", "ko", "zh-Hans", "zh-Hant"],
  "file": ["base.json"]
}
```

| Field | Type | Rule |
| --- | --- | --- |
| `default_language` | string | Required, nonempty |
| `languages` | array<string> | Required, nonempty |
| file | array<string> | Required, nonempty, each item nonempty |

Files resolve exactly as assets/i18n/<language>/<file>. `language` uses standard BCP 47 locales such as `zh-Hans`, without underscore aliases. Missing default in `languages` is appended at runtime.

Each `language` loads/merges listed files in order. Missing files, invalid JSON, unsupported structures fail that `language`. i18n does not enter `Atlas`/`Animation`/`Effect`/Texture/Font/Sound/Music registries.

## Registry Duplicate Summary

| Core configuration | Registry | Combined module check |
| --- | --- | --- |
| Font + fixed size | Font | Yes |
| `sounds` | Sound | Yes |
| `music` | Music | Yes |
| Textures | Texture | Yes |
| `Animation` `Atlas` | `Atlas` | Yes |
| `Animation` | `Animation` | Yes |
| EffectDefinition | `Effect` | Yes |

Identical strings across registries are valid; `Atlas`/`Animation` often deliberately share keys. Only same-registry duplicates fail.
