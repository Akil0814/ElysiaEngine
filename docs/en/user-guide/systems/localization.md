# Localization and Text

This guide covers translation queries, language switching, text measurement, and textures. Translation views and cached textures are borrowed; understand invalidation before retaining them.

`ELYSIA_LOCALIZATION` queries translations, switches languages, measures text, and generates textures. Application/content loading prepares language manifests, translations, and fonts; games do not initialize internal managers.

Register languages/files in the internationalization manifest in the [resource guide](../content/resources.md). `assets/i18n/en/base.json` can contain `{"menu":{"title":"Main menu"}}`, queried as `menu.title`; other languages use the same keys. Leaves must be strings, not arrays/numbers. Language IDs such as en, ja, ko, zh-Hans, and zh-Hant come from the manifest and serve a different purpose from the documentation directory name zh-CN.

## Querying and Switching Languages

Call this function in game settings flows. Obtain IDs from `supported_languages()`, rather than guessing support from directory names.

```cpp
#include "engine/localization/localization_service.h"
#include "engine/tools/logger.h"

#include <string>
#include <expected>
#include <utility>

bool change_language(std::string language)
{
    elysia::localization::LocalizationService* service = ELYSIA_LOCALIZATION;
    const std::expected<void, elysia::localization::LocalizationFailure> result =
        service->set_language(std::move(language));
    if (!result)
    {
        // Log complete failure/origin; false must not report a language change.
        const elysia::localization::LocalizationFailure& failure = result.error();
        elysia::tools::Logger::instance()->log_stream(
            elysia::tools::LogLevel::Error, "localization",
            [&](std::ostream& output)
            {
                output << elysia::core::format_failure_diagnostic(
                    failure.diagnostic, failure.error_code(), "localization");
            }, failure.diagnostic.origin);
        return false;
    }
    return true;
}

// Call after services are ready, not during global variable initialization.
std::string menu_title()
{
    // tr returns a borrowed view; copy to string to retain this result independently.
    return std::string(ELYSIA_LOCALIZATION->tr("menu.title"));
}
```

`set_language` can fail for uninitialized dependencies, unsupported languages, or loading failures; do not assume success. It changes runtime localization, not persisted settings. Persist via [settings scene](../builtin-scenes/settings.md) or [user configuration](../content/configuration.md).

Lookup tries current language, then project default. `engine.` keys can additionally fall back to built-in translations. Missing or uninitialized lookup returns the key itself rather than throwing a missing-translation exception.

`tr()` returns non-owning `std::string_view`. On a miss it may even borrow the input key's storage. Do not retain a view from a temporary key string. Copy for long-term storage and requery after language changes; old text does not translate automatically.

## Measurement and Textures

```cpp
#include "engine/localization/localization_service.h"

// Run after fonts, renderer, and localization are ready.
elysia::localization::LocalizedTextStyle style;
style.wrap_width = 300;
int width = 0;
int height = 0;
if (!ELYSIA_LOCALIZATION->measure_raw_text("Hello", style, width, height))
{
    return; // Place in a void caller handling font/measurement failure.
}
SDL_Texture* texture = ELYSIA_LOCALIZATION->get_raw_text_texture("Hello", style);
if (!texture)
{
    return; // Do not submit invalid textures.
}
// texture is borrowed from the cache; pass it to rendering without destroying it.
```

`get_text_texture(key, style)` accepts translation keys; `get_raw_text_texture(text, style)` accepts raw text. Styles include typography role, optional font source, color, and wrapping width. Texture acquisition may return null; measurement failure returns false.

The engine owns cached textures; never call `SDL_DestroyTexture()`. Successful language changes clear the cache; font updates/cleanup can also invalidate it. Do not retain raw texture pointers across these operations. `font_generation()` helps identify font changes but is not a universal token for every cache invalidation.

For independent ownership, use `CachedTexturePtr` from `create_uncached_raw_text_texture()`, check null, and release before its renderer is destroyed. Independent textures also do not regenerate automatically on language/font changes.

Prefer existing UI text content interfaces so controls handle presentation instead of manually managing textures in every button.

## References

- [Service definition](../../../../engine/localization/localization_service.h)
- [Resources and internationalization](../content/resources.md), [UI guide](../ui/README.md#create-window)
- [Back to user guide](../README.md)
