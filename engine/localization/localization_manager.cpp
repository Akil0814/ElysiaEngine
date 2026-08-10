#include "../tools/logger.h"
#include "../builtin/resources/builtin_asset_cache.h"
#include "localization_manager.h"

#include "../core/render/sdl_convert.h"
#include "../io/json/json_loader.h"
#include "../io/loaders/i18n_manifest_loader.h"
#include "../io/path/path_manager.h"
#include "../typography/font_resolver.h"

#include <SDL_ttf.h>

#include <algorithm>
#include <utility>

namespace elysia::localization
{
namespace
{
using TranslationTable = std::unordered_map<std::string, std::string>;

bool flatten_locale_json(
	const elysia::io::json& node,
	const std::string& prefix,
	TranslationTable& out_table
)
{
	if (node.is_string())
	{
		out_table[prefix] = node.get<std::string>();
		return true;
	}

	if (!node.is_object())
		return false;

	for (auto iterator = node.begin(); iterator != node.end(); ++iterator)
	{
		const std::string key = prefix.empty()
			? iterator.key()
			: prefix + "." + iterator.key();
		if (!flatten_locale_json(iterator.value(), key, out_table))
			return false;
	}

	return true;
}

}

bool LocalizationManager::init(
	SDL_Renderer* renderer,
	const std::filesystem::path& manifest_path,
	std::string initial_language,
	const elysia::typography::FontResolver* font_resolver,
	const elysia::builtin::BuiltinAssetCache* builtin_asset_cache
)
{
	shutdown();

	if (!renderer)
	{
		ELYSIA_LOG_WARN("localization","Localization init failed: renderer is null.");
		return false;
	}
	if (!font_resolver)
	{
		ELYSIA_LOG_WARN("localization",
			"Localization init failed: FontResolver is null.");
		return false;
	}

	elysia::io::I18nManifestLoader manifest_loader;
	if (!manifest_loader.load(manifest_path, _manifest))
	{
		ELYSIA_LOG_WARN("localization","Localization init failed: i18n manifest load failed: "
			<< manifest_path);
		return false;
	}

	if (_manifest.default_language.empty())
	{
		ELYSIA_LOG_WARN("localization","Localization init failed: default language is empty.");
		return false;
	}

	if (_manifest.languages.empty())
	{
		ELYSIA_LOG_WARN("localization","Localization init failed: supported language list is empty.");
		return false;
	}

	if (!is_supported_language(_manifest.default_language))
		_manifest.languages.push_back(_manifest.default_language);

	elysia::io::PathManager* path_manager = elysia::io::PathManager::instance();
	if (!path_manager->is_initialized())
	{
		ELYSIA_LOG_WARN("localization","Localization init failed: path manager is not initialized.");
		return false;
	}

	_renderer = renderer;
	_font_resolver = font_resolver;
	_builtin_asset_cache = builtin_asset_cache;
	_manifest_path = manifest_path;
	_i18n_root = path_manager->assets() / "i18n";

	if (!ensure_language_loaded(_manifest.default_language))
	{
		shutdown();
		return false;
	}

	if (initial_language.empty() || !is_supported_language(initial_language))
		initial_language = _manifest.default_language;

	if (!ensure_language_loaded(initial_language))
		initial_language = _manifest.default_language;

	_current_language = initial_language;
	_initialized = true;
	_text_texture_cache.clear();
	return true;
}

void LocalizationManager::shutdown()
{
	_text_texture_cache.clear();
	_translation_tables.clear();
	_manifest = elysia::io::I18nManifest{};
	_manifest_path.clear();
	_i18n_root.clear();
	_current_language.clear();
	_font_resolver = nullptr;
	_builtin_asset_cache = nullptr;
	_renderer = nullptr;
	_initialized = false;
}

std::string_view LocalizationManager::tr(std::string_view key) const
{
	if (!_initialized)
		return key;

	const auto current_table_iterator = _translation_tables.find(_current_language);
	if (current_table_iterator != _translation_tables.end())
	{
		const std::string_view current_translation =
			lookup_translation(current_table_iterator->second, key);
		if (current_translation != key)
			return current_translation;
	}

	const auto default_table_iterator = _translation_tables.find(_manifest.default_language);
	if (default_table_iterator != _translation_tables.end())
	{
		const std::string_view default_translation =
			lookup_translation(default_table_iterator->second, key);
		if (default_translation != key)
			return default_translation;
	}

	if (key.starts_with("engine.") && _builtin_asset_cache)
	{
		if (const std::string* translation = _builtin_asset_cache->find_translation(_current_language, key))
			return *translation;
		if (_current_language != "en")
		{
			if (const std::string* fallback = _builtin_asset_cache->find_translation("en", key))
				return *fallback;
		}
	}

	return key;
}

SDL_Texture* LocalizationManager::get_text_texture(
	std::string_view key,
	const LocalizedTextStyle& style
)
{
	if (!_initialized)
		return nullptr;

	return _text_texture_cache.get_or_create(
		_current_language,
		_font_resolver ? _font_resolver->generation() : 0,
		key,
		style,
		[this, key, style]()
		{
			return create_text_texture(key, style);
		});
}

SDL_Texture* LocalizationManager::get_raw_text_texture(
	std::string_view text,
	const LocalizedTextStyle& style
)
{
	if (!_initialized)
		return nullptr;

	return _text_texture_cache.get_or_create_raw(
		_current_language,
		_font_resolver ? _font_resolver->generation() : 0,
		text,
		style,
		[this, text, style]()
		{
			return create_raw_text_texture(text, style);
		});
}

CachedTexturePtr LocalizationManager::create_uncached_raw_text_texture(
	std::string_view text,
	const LocalizedTextStyle& style
)
{
	if (!_initialized)
		return {};

	return create_raw_text_texture(text,style);
}

bool LocalizationManager::measure_raw_text(
	std::string_view text,
	const LocalizedTextStyle& style,
	int& out_width,
	int& out_height
) const
{
	out_width = 0;
	out_height = 0;

	if (!_initialized)
		return false;

	TTF_Font* font = resolve_text_font(style);
	if (!font)
		return false;

	if (text.empty())
	{
		out_height = TTF_FontHeight(font);
		return true;
	}

	const std::string raw_text(text);
	if (style.wrap_width > 0)
	{
		// SDL_ttf has no wrapped measurement API. Rendering to a temporary surface is
		// the same layout path used for the final texture, so its extent is authoritative.
		SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(
			font,
			raw_text.c_str(),
			SDL_Color{ 255,255,255,255 },
			style.wrap_width);
		if (!surface)
		{
			ELYSIA_LOG_WARN("localization","Measure wrapped raw text failed, error: " << TTF_GetError());
			return false;
		}

		out_width = surface->w;
		out_height = surface->h;
		SDL_FreeSurface(surface);
		return true;
	}

	if (TTF_SizeUTF8(font,raw_text.c_str(),&out_width,&out_height) != 0)
	{
		ELYSIA_LOG_WARN("localization","Measure raw text failed, error: " << TTF_GetError());
		return false;
	}

	return true;
}

std::uint64_t LocalizationManager::font_generation() const noexcept
{
	return _font_resolver ? _font_resolver->generation() : 0;
}

bool LocalizationManager::set_language(std::string language)
{
	if (!_initialized)
	{
		ELYSIA_LOG_WARN("localization","Set language failed: localization manager is not initialized.");
		return false;
	}

	if (!is_supported_language(language))
	{
		ELYSIA_LOG_WARN("localization","Set language failed: unsupported language: "
			<< language);
		return false;
	}

	if (!ensure_language_loaded(language))
		return false;

	_current_language = std::move(language);
	_text_texture_cache.clear();
	return true;
}

const std::string& LocalizationManager::current_language() const
{
	return _current_language;
}

const std::vector<std::string>& LocalizationManager::supported_languages() const
{
	return _manifest.languages;
}

void LocalizationManager::clear_texture_cache()
{
	_text_texture_cache.clear();
}

bool LocalizationManager::is_supported_language(const std::string& language) const
{
	return std::find(
		_manifest.languages.begin(),
		_manifest.languages.end(),
		language) != _manifest.languages.end();
}

bool LocalizationManager::ensure_language_loaded(const std::string& language)
{
	if (_translation_tables.contains(language))
		return true;

	TranslationTable table;
	if (!load_language_table(language, table))
		return false;

	_translation_tables.emplace(language, std::move(table));
	return true;
}

bool LocalizationManager::load_language_table(
	const std::string& language,
	TranslationTable& out_table
) const
{
	const std::filesystem::path locale_directory = resolve_locale_directory(language);
	if (locale_directory.empty())
	{
		ELYSIA_LOG_WARN("localization","Load language table failed: locale directory not found for "
			<< language);
		return false;
	}

	TranslationTable merged_table;
	for (const std::filesystem::path& relative_file_path : _manifest.files)
	{
		const std::filesystem::path full_file_path = locale_directory / relative_file_path;
		elysia::io::JsonLoader loader;
		const elysia::io::JsonReadResult open_result = loader.open_file(full_file_path);
		if (!open_result.success)
		{
			ELYSIA_LOG_WARN("localization","Load language table failed: " << open_result.error);
			return false;
		}

		if (!flatten_locale_json(loader.root(), "", merged_table))
		{
			ELYSIA_LOG_WARN("localization","Load language table failed: unsupported locale JSON shape: "
				<< full_file_path);
			return false;
		}
	}

	out_table = std::move(merged_table);
	return true;
}

std::filesystem::path LocalizationManager::resolve_locale_directory(
	const std::string& language
) const
{
	const std::filesystem::path direct_path = _i18n_root / language;
	if (std::filesystem::exists(direct_path))
		return direct_path;
	return {};
}

std::string_view LocalizationManager::lookup_translation(
	const TranslationTable& table,
	std::string_view key
) const
{
	const auto iterator = table.find(std::string(key));
	if (iterator == table.end())
		return key;

	return iterator->second;
}

TTF_Font* LocalizationManager::resolve_text_font(
	const LocalizedTextStyle& style) const
{
	if (!_font_resolver)
	{
		ELYSIA_LOG_WARN("localization",
			"Resolve text font failed: FontResolver is unavailable.");
		return nullptr;
	}

	const auto resolved = _font_resolver->resolve_ui(
		style.typography_role,
		_current_language,
		style.font_source_override);
	if (!resolved)
	{
		ELYSIA_LOG_WARN("localization",
			"Resolve text font failed: " << resolved.error().message);
		return nullptr;
	}

	return resolved->font;
}

CachedTexturePtr LocalizationManager::create_text_texture(
	std::string_view key,
	const LocalizedTextStyle& style
)
{
	if (!_renderer)
		return {};

	TTF_Font* font = resolve_text_font(style);
	if (!font)
		return {};

	const std::string translated_text(tr(key));
	if (translated_text.empty())
	{
		ELYSIA_LOG_WARN("localization","Create text texture failed: translated text is empty: "
			<< key);
		return {};
	}

	SDL_Surface* surface = nullptr;
    const SDL_Color text_color = elysia::core::to_sdl_color(style.color);
	if (style.wrap_width > 0)
	{
		surface = TTF_RenderUTF8_Blended_Wrapped(
			font,
			translated_text.c_str(),
			text_color,
			style.wrap_width);
	}
	else
	{
		surface = TTF_RenderUTF8_Blended(
			font,
			translated_text.c_str(),
			text_color);
	}

	if (!surface)
	{
		ELYSIA_LOG_WARN("localization","Create text texture failed: TTF render failed for key "
			<< key << ", error: " << TTF_GetError());
		return {};
	}

	SDL_Texture* texture = SDL_CreateTextureFromSurface(_renderer, surface);
	SDL_FreeSurface(surface);
	if (!texture)
	{
		ELYSIA_LOG_WARN("localization","Create text texture failed: SDL_CreateTextureFromSurface failed for key "
			<< key << ", error: " << SDL_GetError());
		return {};
	}

	return CachedTexturePtr(texture);
}

CachedTexturePtr LocalizationManager::create_raw_text_texture(
	std::string_view text,
	const LocalizedTextStyle& style
)
{
	if (!_renderer)
		return {};

	TTF_Font* font = resolve_text_font(style);
	if (!font)
		return {};

	const std::string raw_text(text);
	if (raw_text.empty())
		return {};

	SDL_Surface* surface = nullptr;
	const SDL_Color text_color = elysia::core::to_sdl_color(style.color);
	if (style.wrap_width > 0)
	{
		surface = TTF_RenderUTF8_Blended_Wrapped(
			font,
			raw_text.c_str(),
			text_color,
			style.wrap_width);
	}
	else
	{
		surface = TTF_RenderUTF8_Blended(
			font,
			raw_text.c_str(),
			text_color);
	}

	if (!surface)
	{
		ELYSIA_LOG_WARN("localization","Create raw text texture failed, error: "
			<< TTF_GetError());
		return {};
	}

	SDL_Texture* texture = SDL_CreateTextureFromSurface(_renderer, surface);
	SDL_FreeSurface(surface);
	if (!texture)
	{
		ELYSIA_LOG_WARN("localization","Create raw text texture failed: SDL_CreateTextureFromSurface failed, error: "
			<< SDL_GetError());
		return {};
	}

	return CachedTexturePtr(texture);
}

}
