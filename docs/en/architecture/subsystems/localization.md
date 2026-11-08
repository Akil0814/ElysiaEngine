# Localization and Text Cache Architecture

This page explains translation, font resolution and text textures; see the [localization guide](../../user-guide/systems/localization.md) for integration. Localization is initialized before project content loading, with project fonts activated after loading succeeds; see the [startup flow](../startup.md).

<a id="localization-service-architecture"></a>
## Localization Service Architecture

The service unifies translation, measurement and text textures; the Manager owns translation tables and caches while borrowing the application FontResolver.

```mermaid
flowchart TB
  Caller["Game code / UI text widgets"]
  Service["LocalizationService / ELYSIA_LOCALIZATION"]
  App["Application<br/>Initialization and shutdown"]
  Manager["LocalizationManager<br/>Current language, translation and text generation"]
  Tables["I18nManifest / TranslationTable<br/>Current-language and default-language fallback"]
  Cache["TextTextureCache<br/>Cache keys include language, style and font generation"]
  Resolver["FontResolver<br/>Font sources and generation"]
  Texture["SDL_ttf / SDL_Renderer<br/>Text measurement and texture creation"]
  Caller -->|Translate, measure, obtain text textures and switch language| Service
  Service -->|Delegate| Manager
  App -->|initialize / shutdown| Manager
  Manager -->|Owns loaded translation tables| Tables
  Manager -->|Owns cache; clears after successful language change| Cache
  Manager -.->|Borrow resolver; obtain font and generation| Resolver
  Manager -->|Create on cache miss or explicit uncached request| Texture
  Cache -.->|Return borrowed textures; invalid after cleanup| Caller
```

Cached textures are borrowed and become invalid after a successful language switch, cache clearing or shutdown. Font generation participates in cache keys to avoid reusing old-font textures. create_uncached_raw_text_texture returns a caller-owned CachedTexturePtr rather than a cache borrow; tr string_view does not extend translation-table lifetime either.

Source: [localization_service.cpp](../../../../engine/localization/localization_service.cpp) · [localization_manager.h](../../../../engine/localization/localization_manager.h) · [text_texture_cache.h](../../../../engine/localization/text_texture_cache.h)

Related: [Service Architecture Overview](../services.md) · [Architecture Index](../README.md).
