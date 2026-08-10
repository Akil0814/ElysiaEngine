#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_asset_cache.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/path/path_manager.h"
#include "engine/loading/content_manifest_pipeline.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/pipeline/resource_request_builder.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"
#include "engine/ui/widgets/ui_text_input.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using elysia::tests::require;

const elysia::core::UiRenderCommand* find_command(
    const std::vector<elysia::core::UiRenderCommand>& commands,
    elysia::core::UiRenderCommandType type)
{
    for (const auto& command : commands)
    {
        if (command.type == type)
            return &command;
    }
    return nullptr;
}

void test_text_input_uses_private_editing_texture()
{
    using namespace elysia;

    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0,
        "text input texture test must initialize SDL video and audio");
    require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
        "text input texture test must initialize PNG support");
    require(TTF_Init() == 0,"text input texture test must initialize SDL_ttf");
    require(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048) == 0,
        "text input texture test must open SDL_mixer audio");

    SDL_Surface* target_surface = SDL_CreateRGBSurfaceWithFormat(
        0,256,128,32,SDL_PIXELFORMAT_RGBA32);
    require(target_surface != nullptr,"text input texture test must create a software target surface");
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(target_surface);
    require(renderer != nullptr,"text input texture test must create a software renderer");

    auto* path_manager = io::PathManager::instance();
    auto* resource_manager = resources::ResourceManager::instance();
    auto* localization_manager = localization::LocalizationManager::instance();
    auto* localization_service = ELYSIA_LOCALIZATION;
    require(path_manager->initialize(),"text input texture test must initialize the project path manager");

    resource_manager->clear();
    const auto resolved_font_settings =
        typography::resolve_font_settings(typography::FontSettings{});
    require(resolved_font_settings.has_value(),
        "text input default font settings must resolve");
    builtin::BuiltinAssetCache engine_cache;
    require(engine_cache.initialize(
        renderer,
        builtin::BuiltinAssetCatalog(*path_manager),
        resolved_font_settings->engine_point_sizes()).has_value(),
        "text input texture test must initialize built-in fonts");
    typography::FontResolver font_resolver;

    localization_manager->shutdown();
    require(localization_manager->initialize(
            renderer,
            path_manager->configs() / "manifests" / "i18n_manifest.json",
            "en",
            &font_resolver,
            &engine_cache),
        "text input texture test must initialize localization");
    require(font_resolver.configure(
        *resolved_font_settings,
        engine_cache,
        *resources::ResourceService::instance(),
        localization_service->supported_languages()).has_value(),
        "text input texture test must configure Engine fonts");

    {
        ui::UiTextInput input(core::Rect{ 0,0,240,48 });
        input.set_text("draft");

        localization::LocalizedTextStyle input_style;
        input_style.typography_role =
            typography::UiTypographyRole::Input;
        input_style.color = input.style().text.enabled;
        SDL_Texture* shared_texture = localization_service->get_raw_text_texture("draft",input_style);
        require(shared_texture != nullptr,"text input texture test must create the comparison cache texture");

        std::vector<core::UiRenderCommand> commands;
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* first_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(first_command && first_command->texture && first_command->texture != shared_texture,
            "input text must bypass the shared raw-text texture cache");
        SDL_Texture* first_private_texture = first_command->texture;

        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* repeated_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(repeated_command && repeated_command->texture == first_private_texture,
            "unchanged input text must reuse its private texture");

        localization_manager->clear_texture_cache();
        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* after_cache_clear_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(after_cache_clear_command && after_cache_clear_command->texture == first_private_texture,
            "clearing the shared cache must not discard unchanged input text");

        input.set_focused(true);
        require(input.on_ui_input_event(ui::UiInputEvent{
                    .type = ui::UiInputEventType::TextEditing,
                    .composition_start = 0,
                    .composition_length = 1,
                    .text = "x"
                }),
            "focused input text must accept an IME composition update");
        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* composition_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(composition_command && composition_command->texture != first_private_texture,
            "IME composition changes must rebuild the private texture");

        SDL_Texture* composition_texture = composition_command->texture;
        input.set_text("changed");
        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* changed_text_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(changed_text_command && changed_text_command->texture != composition_texture,
            "input text changes must rebuild the private texture");

        SDL_Texture* changed_text_texture = changed_text_command->texture;
        input.set_enabled(false);
        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* disabled_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(disabled_command && disabled_command->texture != changed_text_texture,
            "effective text-color changes must rebuild the private texture");
        input.set_enabled(true);
        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* reenabled_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(reenabled_command && reenabled_command->texture,
            "reenabled input text must render with a private texture");
        SDL_Texture* reenabled_texture = reenabled_command->texture;

        input.set_typography_role(
            typography::UiTypographyRole::InputPlaceholder);
        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* typography_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(typography_command && typography_command->texture != reenabled_texture,
            "input typography changes must rebuild the private texture");
        SDL_Texture* typography_texture = typography_command->texture;

        require(localization_service->set_language("zh-Hans"),
            "text input texture test must switch to a loaded language");
        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* language_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(language_command && language_command->texture != typography_texture,
            "language changes must rebuild the private texture");
        require(localization_service->set_language("en"),
            "text input texture test must restore the default language");

        input.set_font_source_override(typography::FontSource::Project);
        commands.clear();
        input.submit_ui_render_commands(commands);
        require(find_command(commands,core::UiRenderCommandType::Texture) == nullptr,
            "an unavailable strict project override must discard the private input texture");
        input.clear_font_source_override();
        commands.clear();
        input.submit_ui_render_commands(commands);
        require(find_command(commands,core::UiRenderCommandType::Texture) != nullptr,
            "clearing a text-input font override must rebuild inherited input text");

        input.clear_text();
        input.set_placeholder_content(ui::ui_raw_text("placeholder"));
        localization::LocalizedTextStyle placeholder_style;
        placeholder_style.typography_role =
            typography::UiTypographyRole::InputPlaceholder;
        placeholder_style.color = input.style().placeholder.enabled;
        SDL_Texture* placeholder_texture = localization_service->get_raw_text_texture("placeholder",placeholder_style);
        require(placeholder_texture != nullptr,"text input texture test must create the placeholder cache texture");

        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* placeholder_command = find_command(commands,core::UiRenderCommandType::Texture);
        require(placeholder_command && placeholder_command->texture == placeholder_texture,
            "text input placeholders must continue using the shared text cache");

        input.set_font_source_override(typography::FontSource::Project);
        commands.clear();
        input.submit_ui_render_commands(commands);
        require(find_command(commands,core::UiRenderCommandType::Texture) == nullptr,
            "text-input placeholders must share the strict font source override");
        input.clear_font_source_override();
        commands.clear();
        input.submit_ui_render_commands(commands);
        const core::UiRenderCommand* restored_placeholder =
            find_command(commands,core::UiRenderCommandType::Texture);
        require(restored_placeholder
                && restored_placeholder->texture == placeholder_texture,
            "clearing the shared override must restore the cached placeholder texture");

        input.reset();
        commands.clear();
        input.submit_ui_render_commands(commands);
        require(find_command(commands,core::UiRenderCommandType::Texture) == nullptr,
            "reset text input must release its private text texture");
    }

    localization_manager->shutdown();
    font_resolver.shutdown();
    resource_manager->clear();
    engine_cache.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(target_surface);
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}
}

int main()
{
    test_text_input_uses_private_editing_texture();
    std::cout << "ui text input rendering tests passed\n";
    return EXIT_SUCCESS;
}
