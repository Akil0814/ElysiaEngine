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
    require(paths->initialize(), "path manager must initialize from the project root");

    auto registry = elysia::io::ContentRegistryLoader{}.load(paths->content_registry());
    require(registry,
        "the minimal content registry must parse");

    elysia::loading::ContentManifestPipeline pipeline;
    auto content_result = pipeline.load(*registry);
    require(content_result,
        "all required manifests in the minimal resource closure must load");
    const auto& content = *content_result;
    require(content.additional_modules.size() == 1
            && content.additional_modules.contains("ryougi_sample"),
        "the standalone example must load only the Ryougi animation sample module");

    elysia::loading::ResourceRequestAssembler assembler;
    auto plan_result = assembler.assemble(content, std::array{10, 20});
    require(plan_result,
        "the minimal manifests must assemble into a valid resource plan");
    const auto& plan = *plan_result;

    require(plan.texture_requests().size() == 1
            && plan.sound_requests().size() == 2
            && plan.music_requests().empty()
            && plan.font_requests().size() == 10
            && plan.atlas_build_requests().size() == 9
            && plan.animation_build_requests().size() == 9
            && plan.animation_effect_build_requests().size() == 1
            && plan.total_request_count() == 32,
        "the plan must contain the reviewed core resources and eight Ryougi animation clips");

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

    const auto* ryougi_idle = find_request(
        plan.atlas_build_requests(), "RyougiShiki.idle",
        [](const auto& request) -> const std::string& { return request.atlas_key; });
    require(ryougi_idle != nullptr
            && ryougi_idle->source_path == paths->textures() / "examples" / "ryougi"
                / "RyougiShiki" / "animation" / "base" / "idle"
            && ryougi_idle->frame_count == 7
            && ryougi_idle->frame_filename_prefix == "RyougiShiki_idle"
            && ryougi_idle->source_type == elysia::resources::AtlasSourceType::FrameDirectory
            && ryougi_idle->origin.module == "ryougi_sample"
            && ryougi_idle->origin.entity_id == "RyougiShiki"
            && ryougi_idle->origin.capability == "animations"
            && !ryougi_idle->origin.segment_index.has_value(),
        "Ryougi idle must preserve its frame-directory path, prefix, and module origin");

    const auto* ryougi_run = find_request(
        plan.animation_build_requests(), "RyougiShiki.run_loop",
        [](const auto& request) -> const std::string& { return request.animation_key; });
    require(ryougi_run != nullptr
            && ryougi_run->atlas_key == "RyougiShiki.run_loop"
            && ryougi_run->fps == 10.0
            && ryougi_run->loop,
        "Ryougi run must remain a ten-FPS looping animation");

    constexpr std::array<std::size_t, 6> attack_frame_counts{7, 7, 8, 14, 9, 12};
    for (std::size_t segment = 0; segment < attack_frame_counts.size(); ++segment)
    {
        const std::string key =
            "RyougiShiki.attack_normal." + std::to_string(segment);
        const std::string filesystem_segment =
            segment < 10 ? "0" + std::to_string(segment) : std::to_string(segment);
        const auto* attack_atlas = find_request(
            plan.atlas_build_requests(), key,
            [](const auto& request) -> const std::string& { return request.atlas_key; });
        const auto* attack_animation = find_request(
            plan.animation_build_requests(), key,
            [](const auto& request) -> const std::string& { return request.animation_key; });

        require(attack_atlas != nullptr
                && attack_atlas->source_path == paths->textures() / "examples" / "ryougi"
                    / "RyougiShiki" / "animation" / "attack" / "normal"
                    / filesystem_segment
                && attack_atlas->frame_count == attack_frame_counts[segment]
                && attack_atlas->frame_filename_prefix
                    == "RyougiShiki_attack_normal_" + filesystem_segment
                && attack_atlas->source_type
                    == elysia::resources::AtlasSourceType::FrameDirectory
                && attack_atlas->origin.module == "ryougi_sample"
                && attack_atlas->origin.segment_index == segment,
            "each Ryougi attack atlas must map its runtime segment to the two-digit frame directory");
        require(attack_animation != nullptr
                && attack_animation->atlas_key == key
                && attack_animation->fps == 10.0
                && !attack_animation->loop
                && attack_animation->segment_index == segment
                && attack_animation->origin.segment_index == segment,
            "each Ryougi attack segment must remain a non-looping ten-FPS animation");
    }

    auto no_fonts = assembler.assemble(content, std::span<const int>{});
    require(no_fonts
            && no_fonts->font_requests().empty()
            && no_fonts->total_request_count() == 22,
        "an empty project font-size set must omit only project font requests");
}
}

int main()
{
    test_minimal_repository_resource_plan();
    std::cout << "resource request assembler tests passed\n";
    return EXIT_SUCCESS;
}
