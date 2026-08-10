#pragma once

#include <string_view>

namespace elysia::builtin::asset_keys
{
	inline constexpr std::string_view LatinFont = "engine.font.latin";
	inline constexpr std::string_view SimplifiedChineseFont = "engine.font.zh_hans";
	inline constexpr std::string_view TraditionalChineseFont = "engine.font.zh_hant";
	inline constexpr std::string_view JapaneseFont = "engine.font.ja";
	inline constexpr std::string_view KoreanFont = "engine.font.ko";

	inline constexpr std::string_view ElysiaDefaultTexture = "engine.brand.elysia.default";
	inline constexpr std::string_view ElysiaBlackTexture = "engine.brand.elysia.black";
	inline constexpr std::string_view ElysiaBlackAlphaInverseTexture = "engine.brand.elysia.black_alpha_inverse";
	inline constexpr std::string_view ElysiaLightEdgeTexture = "engine.brand.elysia.light_edge";
	inline constexpr std::string_view ElysiaWhiteTexture = "engine.brand.elysia.white";
	inline constexpr std::string_view EngineCharacterIdleTexture = "engine.character.sprite.idle";
	inline constexpr std::string_view EngineCharacterMoveTexture = "engine.character.sprite.move";
	inline constexpr std::string_view EngineCharacterIdleAnimation = "engine.character.idle";
	inline constexpr std::string_view EngineCharacterMoveAnimation = "engine.character.move";

	// Reserved Testbed keys. These are intentionally not registered yet.
	inline constexpr std::string_view TestSound = "engine.test.sound";
	inline constexpr std::string_view TestMusic = "engine.test.music";

	inline constexpr std::string_view ElysianRealm = "engine.elysia.music";
}
