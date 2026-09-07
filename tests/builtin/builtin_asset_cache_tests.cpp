#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
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
    elysia::builtin::BuiltinResources resources;
    constexpr std::array default_point_sizes{10,20,30,40,50,60,70};

    const auto initialized = resources.initialize(
        fixture.renderer(),
        catalog,
        default_point_sizes,
        elysia::audio::AudioSettings{
            .master_volume = 125,
            .music_volume = -10,
            .sound_volume = 42
        });
    require(initialized.has_value(), "Built-in asset cache must load all repository built-in resources");
    require(resources.is_initialized(), "successful initialization must publish live built-in resources");
    require(resources.texture_count() == 7, "resources must own all seven Engine textures");
    require(resources.font_count() == 35, "resources must own five font faces at seven fixed sizes");
    require(resources.locale_count() == 5, "resources must own all five Engine translation tables");
    require(resources.find_texture(
            elysia::builtin::BuiltinTextureId::ElysiaWhite) != nullptr,
        "cache must expose the Engine startup logo by stable key");
    require(resources.find_texture(elysia::builtin::BuiltinTextureId::EngineCharacterIdle) != nullptr
            && resources.find_texture(elysia::builtin::BuiltinTextureId::EngineCharacterMove) != nullptr,
        "cache must expose both Engine character sprites by stable key");
    require(resources.find_font(elysia::builtin::BuiltinFontId::SimplifiedChinese, 30) != nullptr,
        "cache must expose Engine fonts by locale and point size");
    require(elysia::builtin::builtin_locale_id("zh_hans")
            == elysia::builtin::BuiltinLocaleId::Count,
        "locale mapping must reject non-BCP-47 locale aliases");
    const std::array legacy_locales{
        std::string("zh") + "_cn",
        std::string("zh_") + "Hans",
        std::string("zh_") + "Hant"
    };
    for (const std::string& legacy_locale : legacy_locales)
    {
        require(elysia::builtin::builtin_locale_id(legacy_locale)
                == elysia::builtin::BuiltinLocaleId::Count,
            "cache must reject every legacy locale spelling");
    }

    elysia::builtin::BuiltinResources custom_size_resources;
    constexpr std::array custom_point_sizes{24};
    require(custom_size_resources.initialize(
            fixture.renderer(),
            catalog,
            custom_point_sizes,
            {}).has_value()
            && custom_size_resources.font_count() == 5
            && custom_size_resources.find_font(
                elysia::builtin::BuiltinFontId::Latin,24) != nullptr,
        "cache must dynamically load Application-requested point sizes");
    custom_size_resources.shutdown();
    require(resources.find_translation(
            elysia::builtin::BuiltinLocaleId::Japanese,
            "engine.settings.title") != nullptr,
        "cache must expose parsed Engine translations");
    require(resources.animation_count() == 2, "resources must register both Engine character animations");
    require(resources.sound_count() == 0 && resources.music_count() == 1,
        "cache must expose the registered Elysia scene music");
    require(resources.find_sound(static_cast<elysia::builtin::BuiltinSoundId>(255)) == nullptr
            && resources.find_music(static_cast<elysia::builtin::BuiltinMusicId>(255)) == nullptr
            && resources.find_music(elysia::builtin::BuiltinMusicId::ElysianRealm) != nullptr,
        "cache must distinguish registered and unregistered Engine audio keys");

    elysia::builtin::BuiltinResources uninitialized_resources;
    require(uninitialized_resources.play_sound(
                static_cast<elysia::builtin::BuiltinSoundId>(255)) == -1
            && !uninitialized_resources.play_music(
                elysia::builtin::BuiltinMusicId::ElysianRealm),
        "unbound built-in audio requests must fail safely");
    require(resources.audio_settings().master_volume == 100
            && resources.audio_settings().music_volume == 0
            && resources.audio_settings().sound_volume == 42,
        "initializing built-in resources must clamp its volume snapshot");
    require(resources.play_sound(static_cast<elysia::builtin::BuiltinSoundId>(255)) == -1
            && !resources.play_music(static_cast<elysia::builtin::BuiltinMusicId>(255)),
        "bound built-in audio requests must not fall back to project resources");
    require(resources.play_music(elysia::builtin::BuiltinMusicId::ElysianRealm),
        "bound built-in audio player must play registered scene music");
    resources.stop_music();
    const auto* idle_definition = resources.find_animation(
        elysia::builtin::BuiltinAnimationId::EngineCharacterIdle);
    const auto* move_definition = resources.find_animation(
        elysia::builtin::BuiltinAnimationId::EngineCharacterMove);
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
    const auto animation = resources.create_animation(
        elysia::builtin::BuiltinAnimationId::EngineCharacterIdle);
    require(animation != nullptr && animation->current_frame_index() == 0
            && animation->current_frame() == first_frame,
        "cache must create an initialized Engine character animation instance");

    elysia::ui::UiAnimation ui_animation(
        elysia::core::Rect{ 0.0f,0.0f,32.0f,32.0f });
    require(ui_animation.set_engine_animation(
                resources,
                elysia::builtin::BuiltinAnimationId::EngineCharacterIdle)
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
    require(resources.find_texture(
            elysia::builtin::BuiltinTextureId::ElysiaWhite) != nullptr
            && resources.find_font(elysia::builtin::BuiltinFontId::Latin, 20) != nullptr,
        "clearing project content must not invalidate built-in resources");
    require(resources.create_animation(
                elysia::builtin::BuiltinAnimationId::EngineCharacterIdle) != nullptr
            && resources.create_animation(
                elysia::builtin::BuiltinAnimationId::EngineCharacterMove) != nullptr,
        "clearing project content must not invalidate built-in animations");
    require(ui_animation.set_engine_animation(
                resources,
                elysia::builtin::BuiltinAnimationId::EngineCharacterIdle),
        "UiAnimation built-in binding must survive project content cleanup");

    const std::filesystem::path missing_root = std::filesystem::temp_directory_path()
        / ("elysia_assist_cache_missing_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto failed_reinitialize = resources.initialize(
        fixture.renderer(),
        elysia::builtin::BuiltinAssetCatalog(missing_root),
        default_point_sizes,
        {});
    require(!failed_reinitialize.has_value(),
        "invalid built-in resources must reject initialization");
    require(resources.texture_count() == 7 && resources.font_count() == 35 && resources.locale_count() == 5
            && resources.animation_count() == 2 && resources.sound_count() == 0
            && resources.music_count() == 1,
        "a failed initialization must preserve the last complete cache transactionally");

    resources.shutdown();
    require(!resources.is_initialized() && resources.texture_count() == 0 && resources.font_count() == 0
            && resources.locale_count() == 0 && resources.animation_count() == 0
            && resources.sound_count() == 0 && resources.music_count() == 0,
        "cache shutdown must release all Engine-owned runtime resources");
    return 0;
}
