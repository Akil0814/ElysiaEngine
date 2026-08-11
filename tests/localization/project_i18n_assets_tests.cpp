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
    "physics_demo_menu.back",
    "physics_demo_menu.collider",
    "physics_demo_menu.platform_tile",
    "physics_demo_menu.title",
    "physics_demo_menu.top_down_tile",
    "testbed.ui_test.typography.description",
    "testbed.ui_test.typography.sample_10",
    "testbed.ui_test.typography.sample_20",
    "testbed.ui_test.typography.sample_30",
    "testbed.ui_test.typography.sample_40",
    "testbed.ui_test.typography.sample_50",
    "testbed.ui_test.typography.sample_60",
    "testbed.ui_test.typography.sample_70",
    "testbed.ui_test.typography.tab",
    "testbed.ui_test.typography.title",
    "testbed.ui_test.status.ready",
    "testbed.ui_test.status.interaction",
    "testbed.ui_test.pages.overview",
    "testbed.ui_test.pages.controls",
    "testbed.ui_test.pages.media",
    "testbed.ui_test.pages.containers",
    "testbed.ui_test.pages.overlays",
    "testbed.ui_test.pages.appearance",
    "testbed.ui_test.sections.overview",
    "testbed.ui_test.sections.controls",
    "testbed.ui_test.sections.media",
    "testbed.ui_test.sections.containers",
    "testbed.ui_test.sections.overlays",
    "testbed.ui_test.sections.appearance",
    "testbed.ui_test.actions.replay",
    "testbed.ui_test.actions.reset",
    "testbed.ui_test.controls.labeled_check",
    "testbed.ui_test.controls.labeled_radio",
    "testbed.ui_test.controls.placeholder",
    "testbed.ui_test.media.long_text",
    "testbed.ui_test.containers.chrome",
    "testbed.ui_test.containers.nested_tab",
    "testbed.ui_test.overlays.open_overlay",
    "testbed.ui_test.overlays.open_dialog",
    "testbed.ui_test.overlays.open_confirm",
    "testbed.ui_test.overlays.tooltip",
    "testbed.ui_test.overlays.tooltip_text",
    "testbed.ui_test.dialog.title",
    "testbed.ui_test.dialog.body",
    "testbed.ui_test.confirm.title",
    "testbed.ui_test.confirm.message",
    "testbed.ui_test.appearance.note",
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
