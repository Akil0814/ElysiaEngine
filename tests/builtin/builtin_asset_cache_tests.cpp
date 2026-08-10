#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_asset_cache.h"
#include "engine/builtin/audio/builtin_audio_player.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/builtin/resources/builtin_asset_keys.h"
#include "engine/io/path/path_manager.h"
#include "engine/loading/content_runtime_cleanup.h"
#include "engine/core/render/colors.h"
#include "engine/ui/widgets/image/ui_animation.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include <chrono>
#include <array>
#include <filesystem>

namespace
{
using elysia::tests::require;

class AssistCacheFixture
{
public:
    AssistCacheFixture()
    {
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0,
            "Built-in asset cache tests must initialize SDL video and audio");
        require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
            "Built-in asset cache tests must initialize PNG support");
        require(TTF_Init() == 0,
            "Built-in asset cache tests must initialize SDL_ttf");
        require(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048) == 0,
            "Built-in asset cache tests must open SDL_mixer audio");

        _surface = SDL_CreateRGBSurfaceWithFormat(0, 128, 128, 32, SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr,
            "Built-in asset cache tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr,
            "Built-in asset cache tests must create a software renderer");
    }

    ~AssistCacheFixture()
    {
        SDL_DestroyRenderer(_renderer);
        SDL_FreeSurface(_surface);
        Mix_CloseAudio();
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    }

    [[nodiscard]] SDL_Renderer* renderer() const noexcept { return _renderer; }

private:
    SDL_Surface* _surface = nullptr;
    SDL_Renderer* _renderer = nullptr;
};
}

int main()
{
    AssistCacheFixture fixture;
    const std::filesystem::path source_root = ELYSIA_SOURCE_DIR;
    elysia::builtin::BuiltinAssetCatalog catalog(source_root);
    elysia::builtin::BuiltinAssetCache cache;
    constexpr std::array default_point_sizes{10,20,30,40,50,60,70};

    const auto initialized = cache.initialize(
        fixture.renderer(),
        catalog,
        default_point_sizes);
    require(initialized.has_value(), "Built-in asset cache must load all repository built-in resources");
    require(cache.initialized(), "successful cache initialization must publish a live cache");
    require(cache.texture_count() == 7, "cache must own all seven Engine textures");
    require(cache.font_count() == 35, "cache must own five font faces at seven fixed sizes");
    require(cache.locale_count() == 5, "cache must own all five Engine translation tables");
    require(cache.find_texture(
            elysia::builtin::asset_keys::ElysiaWhiteTexture) != nullptr,
        "cache must expose the Engine startup logo by stable key");
    require(cache.find_texture(elysia::builtin::asset_keys::EngineCharacterIdleTexture) != nullptr
            && cache.find_texture(elysia::builtin::asset_keys::EngineCharacterMoveTexture) != nullptr,
        "cache must expose both Engine character sprites by stable key");
    require(cache.find_font("zh-Hans", 30) != nullptr,
        "cache must expose Engine fonts by locale and point size");
    require(cache.find_font("zh_hans", 30) == nullptr,
        "cache must reject non-BCP-47 locale aliases");
    const std::array legacy_locales{
        std::string("zh") + "_cn",
        std::string("zh_") + "Hans",
        std::string("zh_") + "Hant"
    };
    for (const std::string& legacy_locale : legacy_locales)
    {
        require(cache.find_font(legacy_locale,30) == nullptr
                && cache.find_translation(legacy_locale,"engine.settings.title") == nullptr,
            "cache must reject every legacy locale spelling");
    }

    elysia::builtin::BuiltinAssetCache custom_size_cache;
    constexpr std::array custom_point_sizes{24};
    require(custom_size_cache.initialize(
            fixture.renderer(),
            catalog,
            custom_point_sizes).has_value()
            && custom_size_cache.font_count() == 5
            && custom_size_cache.find_font("en",24) != nullptr,
        "cache must dynamically load Application-requested point sizes");
    custom_size_cache.shutdown();
    require(cache.find_translation("ja", "engine.settings.title") != nullptr,
        "cache must expose parsed Engine translations");
    require(cache.animation_count() == 2, "cache must register both Engine character animations");
    require(cache.sound_count() == 0 && cache.music_count() == 1,
        "cache must expose the registered Elysia scene music");
    require(cache.find_sound("") == nullptr
            && cache.find_sound(elysia::builtin::asset_keys::TestSound) == nullptr
            && cache.find_music("") == nullptr
            && cache.find_music(elysia::builtin::asset_keys::TestMusic) == nullptr
            && cache.find_music(elysia::builtin::asset_keys::ElysianRealm) != nullptr,
        "cache must distinguish registered and unregistered Engine audio keys");

    elysia::builtin::BuiltinAudioPlayer audio_player;
    require(!audio_player.bound(),
        "Built-in audio player must begin unbound");
    require(audio_player.play_sound(elysia::builtin::asset_keys::TestSound) == -1
            && !audio_player.play_music(elysia::builtin::asset_keys::TestMusic),
        "unbound built-in audio requests must fail safely");
    audio_player.bind(cache,elysia::audio::AudioSettings{
        .master_volume = 125,
        .music_volume = -10,
        .sound_volume = 42
    });
    require(audio_player.bound()
            && audio_player.settings().master_volume == 100
            && audio_player.settings().music_volume == 0
            && audio_player.settings().sound_volume == 42,
        "binding the built-in audio player must clamp its volume snapshot");
    require(audio_player.play_sound(elysia::builtin::asset_keys::TestSound) == -1
            && !audio_player.play_music(elysia::builtin::asset_keys::TestMusic),
        "bound built-in audio requests must not fall back to project resources");
    require(audio_player.play_music(elysia::builtin::asset_keys::ElysianRealm),
        "bound built-in audio player must play registered scene music");
    audio_player.stop_music();
    const auto* idle_definition = cache.find_animation(
        elysia::builtin::asset_keys::EngineCharacterIdleAnimation);
    const auto* move_definition = cache.find_animation(
        elysia::builtin::asset_keys::EngineCharacterMoveAnimation);
    require(idle_definition != nullptr && idle_definition->atlas != nullptr
            && idle_definition->atlas->size() == 8 && idle_definition->fps == 8.0
            && idle_definition->loop
            && move_definition != nullptr && move_definition->atlas != nullptr
            && move_definition->atlas->size() == 8 && move_definition->fps == 8.0
            && move_definition->loop,
        "cache must expose both complete Engine character animation definitions");
    const auto* first_frame = idle_definition->atlas->frame_at(0);
    const auto* last_frame = idle_definition->atlas->frame_at(7);
    require(first_frame != nullptr && last_frame != nullptr
            && first_frame->_coverage_mask != nullptr
            && first_frame->_coverage_mask == last_frame->_coverage_mask
            && first_frame->_source_rect.has_value() && last_frame->_source_rect.has_value()
            && first_frame->_source_rect->width() == 32.0f && first_frame->_source_rect->height() == 32.0f
            && last_frame->_source_rect->x() == 224.0f,
        "Engine character animation atlases must expose eight 32 px source rectangles");
    const auto animation = cache.create_animation(
        elysia::builtin::asset_keys::EngineCharacterIdleAnimation);
    require(animation != nullptr && animation->current_frame_index() == 0
            && animation->current_frame() == first_frame,
        "cache must create an initialized Engine character animation instance");

    elysia::ui::UiAnimation ui_animation(
        elysia::core::Rect{ 0.0f,0.0f,32.0f,32.0f });
    require(ui_animation.set_engine_animation(
                cache,
                elysia::builtin::asset_keys::EngineCharacterIdleAnimation)
            && ui_animation.is_looping(),
        "UiAnimation must bind looping built-in animations without AnimationManager");
    ui_animation.set_opacity(128);
    ui_animation.set_color_overlay(
        elysia::core::Color{
            elysia::core::colors::purple_500.r,
            elysia::core::colors::purple_500.g,
            elysia::core::colors::purple_500.b,
            128 });
    std::vector<elysia::core::UiRenderCommand> ui_commands;
    ui_animation.submit_ui_render_commands(ui_commands);
    require(ui_commands.size() == 2
            && ui_commands[0].texture == first_frame->_texture
            && ui_commands[1].texture == first_frame->_coverage_mask
            && ui_commands[1].src_rect.nearly_equals(ui_commands[0].src_rect)
            && ui_commands[1].alpha == 64
            && ui_commands[1].texture_color_modulation
                == elysia::core::TextureColorModulation{
                    .r = elysia::core::colors::purple_500.r,
                    .g = elysia::core::colors::purple_500.g,
                    .b = elysia::core::colors::purple_500.b },
        "Built-in UiAnimation must render base then a matching color mask");

    elysia::loading::clear_loaded_content();
    require(cache.find_texture(
            elysia::builtin::asset_keys::ElysiaWhiteTexture) != nullptr
            && cache.find_font("en", 20) != nullptr,
        "clearing project content must not invalidate built-in resources");
    require(cache.create_animation(
                elysia::builtin::asset_keys::EngineCharacterIdleAnimation) != nullptr
            && cache.create_animation(
                elysia::builtin::asset_keys::EngineCharacterMoveAnimation) != nullptr,
        "clearing project content must not invalidate built-in animations");
    require(ui_animation.set_engine_animation(
                cache,
                elysia::builtin::asset_keys::EngineCharacterIdleAnimation),
        "UiAnimation built-in binding must survive project content cleanup");

    const std::filesystem::path missing_root = std::filesystem::temp_directory_path()
        / ("elysia_assist_cache_missing_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto failed_reinitialize = cache.initialize(
        fixture.renderer(),
        elysia::builtin::BuiltinAssetCatalog(missing_root),
        default_point_sizes);
    require(!failed_reinitialize.has_value(),
        "invalid built-in resources must reject initialization");
    require(cache.texture_count() == 7 && cache.font_count() == 35 && cache.locale_count() == 5
            && cache.animation_count() == 2 && cache.sound_count() == 0
            && cache.music_count() == 1,
        "a failed initialization must preserve the last complete cache transactionally");

    audio_player.unbind();
    require(!audio_player.bound(), "unbinding must detach the built-in audio player");
    cache.shutdown();
    require(!cache.initialized() && cache.texture_count() == 0 && cache.font_count() == 0
            && cache.locale_count() == 0 && cache.animation_count() == 0
            && cache.sound_count() == 0 && cache.music_count() == 0,
        "cache shutdown must release all Engine-owned runtime resources");
    return 0;
}
