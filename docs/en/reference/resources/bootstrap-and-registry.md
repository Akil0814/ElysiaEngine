# Bootstrap Registry and Preload

This document explains startup registry parsing and ownership of built-in resources/project preload `textures`. Establish loading phases/ownership before inspecting fields.

Project roots require ordinary file `assets/.elysia_root`; `content_registry.json` cannot replace it. Default entry is `assets/content_registry.json`. Bootstrap parses once at startup; `Application` owns the read-only `ContentRegistry` snapshot. Scenes borrow through `SceneRuntimeContext` and loading does not reread it.

Root requires exactly `bootstrap`/`manifests`. Duplicate properties reject before parsing; unknown fields/missing target files return structured startup failures.

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

## `bootstrap`

| Field | Rule |
| --- | --- |
| `app_config` | Required string, relative to `assets/`; fixed `AppConfig` schema read by Bootstrap. |
| `preload_manifest` | Required string, relative to `assets/`; startup texture preload. |

No other `bootstrap` fields are accepted. Generic game configuration is not loaded at startup.

## `manifests.required`

Required must be an object with all seven entries:

| Field | Target |
| --- | --- |
| `configs` | Generic game configuration manifest; snapshot built during content loading. |
| `fonts` | Core font manifest. |
| `audio` | Core Sound/Music manifest. |
| `i18n` | Internationalization manifest. |
| `textures` | Core `textures`. |
| `animations` | Core `Animation` manifest. |
| `effects` | Core EffectDefinition manifest. |

Each value is a string resolving to an ordinary file under `assets/`. `ContentManifestPipeline` reads declarations; `GameContentLoader` publishes `configs`' `ConfigSnapshot` only after successful registration of `Atlas`, `textures`, `fonts`, `audio`, `Animation`, EffectDefinition.

## `manifests.additional`

Optional `additional` maps module names to manifest paths. Names do not select loaders; every module follows one entity-pack schema and stable name sorting. See [entity modules](entity-content.md).

## `preload_manifest.json`

Preload reads explicit `textures` entries. Standalone examples need no logo and use an empty array:

```json
{
  "textures": []
}
```

Each project startup texture needs a stable key/relative filename under `assets/preload/`. All entries are `required`; missing/undecodable files or SDL texture creation fail startup stage two without publishing partial caches.

`BuiltinResources` singleton manages `assets/engine`. After renderer creation, `Application` `initialize()` transactionally loads seven Engine `textures`, five configured-size font sets, five-language `engine.json`, two `animations`, one music track. They remain until shutdown and never register with project `ResourceManager`.

Query fixed resources via `BuiltinTextureId`, `BuiltinFontId`, `BuiltinAnimationId`, `BuiltinSoundId`, `BuiltinMusicId` enums. Stable strings serve logs/reserved-name validation only. `.elysia_engine_required` is `required`; `OFL.txt` is license only. Project cleanup/reload does not affect built-ins.

`BuiltinResources` reuses Singleton<T>; instance() returns one pointer. First access constructs an empty facade without SDL loading. Engine/project code may include resource headers directly, without passing through registration context, constructors, or `SceneRuntimeContext`. IGameModule::register_scenes only registers.

`Application` owns runtime lifetime: close `SceneManager`/localization/`fonts` first, then explicit `BuiltinResources::shutdown()`, then renderer/TTF/mixer. Shutdown is repeatable and reinitialization supported; normal release does not rely on static process-exit destruction. Only one active `Application` is supported.

`BootstrapTextureCache` owns only project preload `textures`. find_preload_texture returns nullable borrows without logging. Validity ends at `StartupLoadingScene::on_exit()`; application shutdown retains idempotent fallback cleanup.

## Built-In Music Playback

`BuiltinResources` only owns/queries; it has no playback/volume APIs. Separate singleton `BuiltinMusicPlayer` plays music through explicit `engine/builtin/audio/builtin_music_player.h`. First access creates no SDL resources; `Application` initializes after built-in loading.

It shares the mixer device with project `audio` but owns a separate track and queries only `BuiltinMusicId`, without public-pool fallback. play(id,loops,fade_in) uses native SDL_mixer fades and defaults looping; absent/nonpositive fade plays immediately. Intro fades in for 1500 milliseconds; handoff to Realm preserves playback. stop immediately stops/unbinds, with no fade-out or queue.

`Application` synchronizes common master/music settings; changes apply immediately preserving fade progress. Invalid IDs/uninitialized requests fail without affecting current play. Valid requests, including the same music, restart; backend failures stop/unbind.

Shutdown closes scenes, stops/destroys built-in tracks, releases resources, then closes mixer. Explicit built-in reinitialization/shutdown also stops/unbinds; project cleanup does not affect built-in music.
