#pragma once

#include "localized_text_style.h"
#include "text_texture_cache.h"
#include "../io/loaders/asset_config_types.h"
#include "../tools/singleton.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <filesystem>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::builtin
{
class BuiltinAssetCache;
}

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::localization
{
class LocalizationService;

class LocalizationManager : public elysia::tools::Singleton<LocalizationManager>
{
	friend elysia::tools::Singleton<LocalizationManager>;
	friend class LocalizationService;

public:
	bool init(
		SDL_Renderer* renderer,
		const std::filesystem::path& manifest_path,
		std::string initial_language,
		const elysia::typography::FontResolver* font_resolver,
		const elysia::builtin::BuiltinAssetCache* builtin_asset_cache = nullptr
	);
	void shutdown();
	void clear_texture_cache();

private:
	std::string_view tr(std::string_view key) const;
	SDL_Texture* get_text_texture(std::string_view key, const LocalizedTextStyle& style);
	SDL_Texture* get_raw_text_texture(std::string_view text, const LocalizedTextStyle& style);
	// Creates an owning raw-text texture without inserting it into TextTextureCache.
	[[nodiscard]] CachedTexturePtr create_uncached_raw_text_texture(
		std::string_view text,
		const LocalizedTextStyle& style
	);
	bool measure_raw_text(std::string_view text,const LocalizedTextStyle& style,int& out_width,int& out_height) const;
	[[nodiscard]] std::uint64_t font_generation() const noexcept;

	bool set_language(std::string language);
	const std::string& current_language() const;
	const std::vector<std::string>& supported_languages() const;

	using TranslationTable = std::unordered_map<std::string, std::string>;

	bool is_supported_language(const std::string& language) const;
	bool ensure_language_loaded(const std::string& language);
	bool load_language_table(const std::string& language, TranslationTable& out_table) const;
	std::filesystem::path resolve_locale_directory(const std::string& language) const;
	std::string_view lookup_translation(
		const TranslationTable& table,
		std::string_view key
	) const;
	TTF_Font* resolve_text_font(const LocalizedTextStyle& style) const;
	CachedTexturePtr create_text_texture(
		std::string_view key,
		const LocalizedTextStyle& style
	);
	CachedTexturePtr create_raw_text_texture(
		std::string_view text,
		const LocalizedTextStyle& style
	);

private:
	SDL_Renderer* _renderer = nullptr;
	std::filesystem::path _manifest_path;
	std::filesystem::path _i18n_root;
	elysia::io::I18nManifest _manifest;
	TextTextureCache _text_texture_cache;
	std::unordered_map<std::string, TranslationTable> _translation_tables;
	std::string _current_language;
	const elysia::typography::FontResolver* _font_resolver = nullptr;
	const elysia::builtin::BuiltinAssetCache* _builtin_asset_cache = nullptr;
	bool _initialized = false;
};

}
