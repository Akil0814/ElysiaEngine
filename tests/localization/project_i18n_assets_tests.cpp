// Project-owned translation asset contract.
#include "engine/io/json/strict_json.h"
#include "tests/support/test_assertions.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using elysia::tests::require;

bool is_valid_utf8(std::string_view value)
{
    for (std::size_t index = 0; index < value.size();)
    {
        const auto first = static_cast<std::uint8_t>(value[index]);
        if (first <= 0x7f)
        {
            ++index;
            continue;
        }

        std::size_t continuation_count = 0;
        std::uint8_t minimum_second = 0x80;
        std::uint8_t maximum_second = 0xbf;
        if (first >= 0xc2 && first <= 0xdf)
            continuation_count = 1;
        else if (first == 0xe0)
        {
            continuation_count = 2;
            minimum_second = 0xa0;
        }
        else if (first >= 0xe1 && first <= 0xec)
            continuation_count = 2;
        else if (first == 0xed)
        {
            continuation_count = 2;
            maximum_second = 0x9f;
        }
        else if (first >= 0xee && first <= 0xef)
            continuation_count = 2;
        else if (first == 0xf0)
        {
            continuation_count = 3;
            minimum_second = 0x90;
        }
        else if (first >= 0xf1 && first <= 0xf3)
            continuation_count = 3;
        else if (first == 0xf4)
        {
            continuation_count = 3;
            maximum_second = 0x8f;
        }
        else
            return false;

        if (index + continuation_count >= value.size())
            return false;
        const auto second = static_cast<std::uint8_t>(value[index + 1]);
        if (second < minimum_second || second > maximum_second)
            return false;
        for (std::size_t offset = 2; offset <= continuation_count; ++offset)
        {
            const auto continuation = static_cast<std::uint8_t>(value[index + offset]);
            if (continuation < 0x80 || continuation > 0xbf)
                return false;
        }
        index += continuation_count + 1;
    }
    return true;
}

bool is_snake_case_segment(std::string_view segment)
{
    if (segment.empty() || segment.front() == '_' || segment.back() == '_')
        return false;
    for (const char character : segment)
    {
        const bool valid = (character >= 'a' && character <= 'z')
            || (character >= '0' && character <= '9')
            || character == '_';
        if (!valid)
            return false;
    }
    return segment.find("__") == std::string_view::npos;
}

void collect_leaf_keys(const elysia::io::json& value,
                       const std::string& prefix,
                       std::set<std::string>& keys)
{
    if (!value.is_object())
    {
        require(value.is_string(), "project i18n leaves must be strings");
        require(!value.get_ref<const std::string&>().empty(),
            "project i18n strings must not be empty");
        keys.insert(prefix);
        return;
    }

    require(!value.empty(), "project i18n objects must not be empty");
    for (const auto& [key, child] : value.items())
    {
        require(is_snake_case_segment(key),
            "project i18n key segments must use lowercase snake_case");
        collect_leaf_keys(child,prefix.empty() ? key : prefix + "." + key,keys);
    }
}

const std::set<std::string> expected_keys = {
    "common.back",
    "common.cancel",
    "common.close",
    "common.coming_soon",
    "common.confirm",
    "common.save",
    "animation_preview.animation_idle",
    "animation_preview.animation_run",
    "animation_preview.attack_next",
    "animation_preview.attack_previous",
    "animation_preview.attack_replay",
    "animation_preview.attack_segment",
    "animation_preview.back",
    "animation_preview.description",
    "animation_preview.title",
    "demo_gallery.animation_preview",
    "demo_gallery.back",
    "demo_gallery.elysia_realm",
    "demo_gallery.engine_features",
    "demo_gallery.failure_confirm.cancel",
    "demo_gallery.failure_confirm.close",
    "demo_gallery.failure_confirm.confirm",
    "demo_gallery.failure_confirm.message",
    "demo_gallery.failure_confirm.title",
    "demo_gallery.failure_test",
    "demo_gallery.physics",
    "demo_gallery.title",
    "demo_gallery.ui_gallery",
    "menu_scene.demo_gallery",
    "menu_scene.exit",
    "menu_scene.exit_confirm.cancel",
    "menu_scene.exit_confirm.close",
    "menu_scene.exit_confirm.message",
    "menu_scene.exit_confirm.title",
    "menu_scene.project_name",
    "menu_scene.settings",
    "physics_combat_gallery.back",
    "physics_combat_gallery.collider",
    "physics_combat_gallery.platform_tile",
    "physics_combat_gallery.title",
    "physics_combat_gallery.top_down_tile",
    "ui_component_gallery.typography.description",
    "ui_component_gallery.typography.sample_10",
    "ui_component_gallery.typography.sample_20",
    "ui_component_gallery.typography.sample_30",
    "ui_component_gallery.typography.sample_40",
    "ui_component_gallery.typography.sample_50",
    "ui_component_gallery.typography.sample_60",
    "ui_component_gallery.typography.sample_70",
    "ui_component_gallery.typography.tab",
    "ui_component_gallery.typography.title",
    "ui_component_gallery.status.ready",
    "ui_component_gallery.status.interaction",
    "ui_component_gallery.status.submitted",
    "ui_component_gallery.status.overlay_opened",
    "ui_component_gallery.status.selection_changed",
    "ui_component_gallery.pages.overview",
    "ui_component_gallery.pages.states",
    "ui_component_gallery.pages.controls",
    "ui_component_gallery.pages.media",
    "ui_component_gallery.pages.containers",
    "ui_component_gallery.pages.overlays",
    "ui_component_gallery.pages.appearance",
    "ui_component_gallery.sections.overview",
    "ui_component_gallery.sections.controls",
    "ui_component_gallery.sections.media",
    "ui_component_gallery.sections.containers",
    "ui_component_gallery.sections.overlays",
    "ui_component_gallery.sections.appearance",
    "ui_component_gallery.actions.replay",
    "ui_component_gallery.actions.reset",
    "ui_component_gallery.actions.replay_effects",
    "ui_component_gallery.actions.play",
    "ui_component_gallery.actions.pause",
    "ui_component_gallery.actions.resume",
    "ui_component_gallery.actions.reset_animation",
    "ui_component_gallery.controls.labeled_check",
    "ui_component_gallery.controls.labeled_radio",
    "ui_component_gallery.controls.labeled_radio_far",
    "ui_component_gallery.controls.placeholder",
    "ui_component_gallery.states.buttons_title",
    "ui_component_gallery.states.buttons_description",
    "ui_component_gallery.states.text_button",
    "ui_component_gallery.states.role_button",
    "ui_component_gallery.states.disabled",
    "ui_component_gallery.states.selection_title",
    "ui_component_gallery.states.selection_description",
    "ui_component_gallery.states.presentation_title",
    "ui_component_gallery.states.presentation_description",
    "ui_component_gallery.states.fit_sample",
    "ui_component_gallery.states.text_block_sample",
    "ui_component_gallery.states.panel_sample",
    "ui_component_gallery.states.inputs_title",
    "ui_component_gallery.states.inputs_description",
    "ui_component_gallery.media.long_text",
    "ui_component_gallery.media.text_title",
    "ui_component_gallery.media.text_description",
    "ui_component_gallery.media.number_title",
    "ui_component_gallery.media.number_description",
    "ui_component_gallery.media.effects_title",
    "ui_component_gallery.media.effects_description",
    "ui_component_gallery.media.fade_in",
    "ui_component_gallery.media.fade_in_out",
    "ui_component_gallery.media.blink_visible",
    "ui_component_gallery.media.pulse_min",
    "ui_component_gallery.media.animation_title",
    "ui_component_gallery.media.animation_description",
    "ui_component_gallery.containers.chrome",
    "ui_component_gallery.containers.nested_tab",
    "ui_component_gallery.containers.nested_tab_two",
    "ui_component_gallery.containers.list_title",
    "ui_component_gallery.containers.list_description",
    "ui_component_gallery.containers.layout_title",
    "ui_component_gallery.containers.layout_description",
    "ui_component_gallery.containers.scroll_title",
    "ui_component_gallery.containers.scroll_description",
    "ui_component_gallery.overlays.open_overlay",
    "ui_component_gallery.overlays.open_dialog",
    "ui_component_gallery.overlays.open_confirm",
    "ui_component_gallery.overlays.tooltip",
    "ui_component_gallery.overlays.tooltip_text",
    "ui_component_gallery.overlays.placement_title",
    "ui_component_gallery.overlays.placement_description",
    "ui_component_gallery.overlays.open_center",
    "ui_component_gallery.overlays.open_left",
    "ui_component_gallery.overlays.open_right",
    "ui_component_gallery.overlays.open_top",
    "ui_component_gallery.overlays.open_bottom",
    "ui_component_gallery.overlays.dialog_title",
    "ui_component_gallery.overlays.dialog_description",
    "ui_component_gallery.overlays.popup_title",
    "ui_component_gallery.overlays.popup_description",
    "ui_component_gallery.overlays.option_four",
    "ui_component_gallery.overlays.option_five",
    "ui_component_gallery.overlays.option_six",
    "ui_component_gallery.overlays.option_seven",
    "ui_component_gallery.overlays.option_eight",
    "ui_component_gallery.dialog.title",
    "ui_component_gallery.dialog.body",
    "ui_component_gallery.confirm.title",
    "ui_component_gallery.confirm.message",
    "ui_component_gallery.appearance.note",
    "ui_component_gallery.appearance.localized_sample",
};
}

int main()
{
    const std::filesystem::path source_root = ELYSIA_SOURCE_DIR;
    const std::filesystem::path i18n_directory = source_root / "assets" / "i18n";
    const std::set<std::string> locales = {"en","ja","ko","zh-Hans","zh-Hant"};

    const auto manifest = elysia::io::load_strict_json(
        source_root / "assets" / "configs" / "manifests" / "i18n_manifest.json");
    require(manifest.has_value(), "project i18n manifest must be valid strict JSON");
    require(manifest->at("default_language") == "en",
        "project i18n default locale must be English");
    require(manifest->at("languages").get<std::vector<std::string>>()
            == std::vector<std::string>{"en","ja","ko","zh-Hans","zh-Hant"},
        "project i18n manifest must list the five canonical locales");
    require(manifest->at("file").get<std::vector<std::string>>()
            == std::vector<std::string>{"base.json"},
        "project i18n manifest must load the base translation file");

    std::set<std::string> actual_directories;
    for (const auto& entry : std::filesystem::directory_iterator(i18n_directory))
    {
        if (entry.is_directory())
            actual_directories.insert(entry.path().filename().string());
    }
    require(actual_directories == locales,
        "project i18n directory names must exactly match the canonical locales");

    std::set<std::string> reference_keys;
    for (const std::string& locale : locales)
    {
        const std::filesystem::path file_path = i18n_directory / locale / "base.json";
        std::ifstream input(file_path,std::ios::binary);
        require(input.is_open(), "project locale file must be readable");
        const std::string raw_content{
            std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        require(is_valid_utf8(raw_content), "project locale file must be valid UTF-8");
        const std::string legacy_physics_key =
            std::string{"physics_demo"} + "_menu";
        const std::string legacy_ui_key =
            std::string{"testbed"} + ".ui_test";
        const std::string legacy_root =
            std::string{"\""} + "testbed" + "\"";
        require(raw_content.find(legacy_physics_key) == std::string::npos
                && raw_content.find(legacy_ui_key) == std::string::npos
                && raw_content.find(legacy_root) == std::string::npos,
            "project locale files must not retain legacy demo key prefixes");

        const auto document = elysia::io::load_strict_json(file_path);
        require(document.has_value() && document->is_object(),
            "project locale file must be a strict JSON object");
        std::set<std::string> keys;
        collect_leaf_keys(*document,"",keys);
        require(keys == expected_keys,
            "project locale key tree must match the project translation contract");
        if (reference_keys.empty())
            reference_keys = keys;
        else
            require(keys == reference_keys,
                "all project locales must use the same key tree");
    }
    return 0;
}
