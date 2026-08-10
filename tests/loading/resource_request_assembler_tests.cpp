#define SDL_MAIN_HANDLED

#include "engine/io/loaders/content_registry_loader.h"
#include "engine/io/path/path_manager.h"
#include "engine/loading/content_manifest_pipeline.h"
#include "engine/loading/resource_load_plan.h"
#include "engine/loading/resource_request_assembler.h"
#include "engine/resources/resource_types.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
using elysia::tests::require;

template<typename Request, typename KeyProjection>
const Request* find_request(
    const std::vector<Request>& requests,
    std::string_view key,
    KeyProjection project_key)
{
    const auto position = std::find_if(
        requests.begin(), requests.end(),
        [key, project_key](const Request& request)
        {
            return project_key(request) == key;
        });
    return position == requests.end() ? nullptr : &*position;
}

void test_minimal_repository_resource_plan()
{
    auto* paths = elysia::io::PathManager::instance();
    require(paths->init(), "path manager must initialize from the project root");

    elysia::io::ContentRegistry registry;
    require(elysia::io::ContentRegistryLoader{}.load(paths->content_registry(), registry),
        "the minimal content registry must parse");

    elysia::loading::ContentManifestResult content;
    elysia::loading::ContentManifestPipeline pipeline;
    require(pipeline.load(registry, content),
        "all required manifests in the minimal resource closure must load");
    require(content.additional_modules.empty(),
        "the standalone example must not load project-specific content modules");

    elysia::loading::ResourceLoadPlan plan;
    elysia::loading::ResourceRequestAssembler assembler;
    require(assembler.assemble(content, std::array{10, 20}, plan),
        "the minimal manifests must assemble into a valid resource plan");

    require(plan.texture_requests().size() == 1
            && plan.sound_requests().size() == 2
            && plan.music_requests().empty()
            && plan.font_requests().size() == 10
            && plan.atlas_build_requests().size() == 1
            && plan.animation_build_requests().size() == 1
            && plan.animation_effect_build_requests().size() == 1
            && plan.total_request_count() == 16,
        "the plan must contain only the reviewed standalone example resources");

    const auto* texture = find_request(
        plan.texture_requests(), "ui.moon",
        [](const auto& request) -> const std::string& { return request.key; });
    require(texture != nullptr
            && texture->file_path == paths->textures() / "ui" / "moon.png"
            && texture->origin.config_path.filename() == "textures_manifest.json"
            && texture->origin.capability == "textures",
        "ui.moon must preserve its key, path, and manifest origin");

    for (const std::string_view key : {
            "system.button_click_down", "system.button_click_up" })
    {
        const auto* sound = find_request(
            plan.sound_requests(), key,
            [](const auto& request) -> const std::string& { return request.key; });
        require(sound != nullptr
                && sound->file_path == paths->audio() / "system"
                    / (std::string{key.substr(key.rfind('.') + 1)} + ".wav")
                && sound->origin.config_path.filename() == "audio_manifest.json"
                && sound->origin.capability == "sounds",
            "each button sound must preserve its key, path, and manifest origin");
    }

    for (const std::string_view family : {
            "ui.latin", "ui.zh_hans", "ui.zh_hant", "ui.ja", "ui.ko" })
    {
        for (const int point_size : {10, 20})
        {
            const std::string key = std::string{family} + "." + std::to_string(point_size);
            const auto* font = find_request(
                plan.font_requests(), key,
                [](const auto& request) -> const std::string& { return request.key; });
            require(font != nullptr
                    && font->point_size == point_size
                    && font->file_path.parent_path() == paths->fonts()
                    && font->origin.config_path.filename() == "fonts_manifest.json",
                "each project font family must expand at every requested point size");
        }
    }

    const auto* atlas = find_request(
        plan.atlas_build_requests(), "test.animation",
        [](const auto& request) -> const std::string& { return request.atlas_key; });
    require(atlas != nullptr
            && atlas->source_path == paths->textures() / "test" / "frame_group.png"
            && atlas->frame_count == 14
            && atlas->source_type == elysia::resources::AtlasSourceType::HorizontalStrip
            && atlas->origin.config_path.filename() == "animations_manifest.json",
        "test.animation must describe the fourteen-frame horizontal strip");

    const auto* animation = find_request(
        plan.animation_build_requests(), "test.animation",
        [](const auto& request) -> const std::string& { return request.animation_key; });
    require(animation != nullptr
            && animation->atlas_key == "test.animation"
            && animation->fps == 10.0
            && animation->loop,
        "test.animation must preserve its playback configuration");

    const auto* effect = find_request(
        plan.animation_effect_build_requests(), "effect.test",
        [](const auto& request) -> const std::string& { return request.effect_key; });
    require(effect != nullptr
            && effect->animation_key == "test.animation"
            && effect->origin.config_path.filename() == "effects_manifest.json",
        "effect.test must bind the only example animation");

    elysia::loading::ResourceLoadPlan no_fonts;
    require(assembler.assemble(content, std::span<const int>{}, no_fonts)
            && no_fonts.font_requests().empty()
            && no_fonts.total_request_count() == 6,
        "an empty project font-size set must omit only project font requests");
}
}

int main()
{
    test_minimal_repository_resource_plan();
    std::cout << "resource request assembler tests passed\n";
    return EXIT_SUCCESS;
}
