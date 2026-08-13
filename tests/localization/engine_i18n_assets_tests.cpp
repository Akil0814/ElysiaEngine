// Engine-owned translation asset contract.
#include "engine/io/json/strict_json.h"
#include "tests/support/test_assertions.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>

namespace
{

using elysia::tests::require;

bool is_valid_utf8(const std::string_view value)
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
        {
            continuation_count = 1;
        }
        else if (first == 0xe0)
        {
            continuation_count = 2;
            minimum_second = 0xa0;
        }
        else if (first >= 0xe1 && first <= 0xec)
        {
            continuation_count = 2;
        }
        else if (first == 0xed)
        {
            continuation_count = 2;
            maximum_second = 0x9f;
        }
        else if (first >= 0xee && first <= 0xef)
        {
            continuation_count = 2;
        }
        else if (first == 0xf0)
        {
            continuation_count = 3;
            minimum_second = 0x90;
        }
        else if (first >= 0xf1 && first <= 0xf3)
        {
            continuation_count = 3;
        }
        else if (first == 0xf4)
        {
            continuation_count = 3;
            maximum_second = 0x8f;
        }
        else
        {
            return false;
        }

        if (index + continuation_count >= value.size())
        {
            return false;
        }

        const auto second = static_cast<std::uint8_t>(value[index + 1]);
        if (second < minimum_second || second > maximum_second)
        {
            return false;
        }

        for (std::size_t offset = 2; offset <= continuation_count; ++offset)
        {
            const auto continuation = static_cast<std::uint8_t>(value[index + offset]);
            if (continuation < 0x80 || continuation > 0xbf)
            {
                return false;
            }
        }

        index += continuation_count + 1;
    }

    return true;
}

void collect_leaf_keys(const elysia::io::json& value,
                       const std::string& prefix,
                       std::set<std::string>& keys)
{
    if (!value.is_object())
    {
        require(value.is_string(), "engine i18n leaves must be strings");
        require(!value.get_ref<const std::string&>().empty(), "engine i18n strings must not be empty");
        keys.insert(prefix);
        return;
    }

    require(!value.empty(), "engine i18n objects must not be empty");
    for (const auto& [key, child] : value.items())
    {
        const std::string path = prefix.empty() ? key : prefix + "." + key;
        collect_leaf_keys(child, path, keys);
    }
}

const std::set<std::string> expected_keys = {
    "engine.application_failure.copy",
    "engine.application_failure.copied",
    "engine.application_failure.copy_failed",
    "engine.application_failure.details_in_log",
    "engine.application_failure.labels.details",
    "engine.application_failure.labels.error_code",
    "engine.application_failure.labels.log",
    "engine.application_failure.log_unavailable",
    "engine.application_failure.runtime.exit",
    "engine.application_failure.runtime.message",
    "engine.application_failure.runtime.reopen",
    "engine.application_failure.runtime.title",
    "engine.application_failure.startup.exit",
    "engine.application_failure.startup.message",
    "engine.application_failure.startup.title",
    "engine.application_failure.stage.animation",
    "engine.application_failure.stage.atlas",
    "engine.application_failure.stage.audio",
    "engine.application_failure.stage.config",
    "engine.application_failure.stage.effect",
    "engine.application_failure.stage.font",
    "engine.application_failure.stage.manifest",
    "engine.application_failure.stage.missing_resource",
    "engine.application_failure.stage.plan",
    "engine.application_failure.stage.texture",
    "engine.common.back",
    "engine.common.cancel",
    "engine.common.close",
    "engine.common.close_x",
    "engine.common.confirm",
    "engine.common.save",
    "engine.settings.actions.back",
    "engine.settings.actions.save",
    "engine.settings.fields.window_mode",
    "engine.settings.fields.language",
    "engine.settings.fields.master_volume",
    "engine.settings.fields.music_volume",
    "engine.settings.fields.window_size",
    "engine.settings.fields.sound_volume",
    "engine.settings.languages.en",
    "engine.settings.languages.ja",
    "engine.settings.languages.ko",
    "engine.settings.languages.zh_hans",
    "engine.settings.languages.zh_hant",
    "engine.settings.sections.audio",
    "engine.settings.sections.display",
    "engine.settings.sections.general",
    "engine.settings.status.saved",
    "engine.settings.window_modes.borderless_fullscreen",
    "engine.settings.title",
    "engine.startup_loading.press_any_button",
};

} // namespace

int main()
{
    const std::filesystem::path i18n_directory =
        std::filesystem::path(ELYSIA_SOURCE_DIR) / "assets" / "engine" / "i18n";
    const std::set<std::string> locales = {"en", "zh-Hans", "zh-Hant", "ja", "ko"};
    std::set<std::string> reference_keys;

    for (const auto& locale : locales)
    {
        const std::filesystem::path file_path = i18n_directory / locale / "engine.json";
        require(std::filesystem::exists(file_path), "engine locale file must exist");

        std::ifstream input(file_path, std::ios::binary);
        require(input.is_open(), "engine locale file must be readable");
        const std::string raw_content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        require(is_valid_utf8(raw_content), "engine locale file must be valid UTF-8");

        const auto document = elysia::io::load_strict_json(file_path);
        require(document.has_value(), "engine locale file must be valid strict JSON");
        require(document->is_object() && document->size() == 1 && document->contains("engine"),
                "engine locale must contain only the engine namespace");

        std::set<std::string> keys;
        collect_leaf_keys(*document, "", keys);
        require(keys == expected_keys, "engine locale key tree must match the engine contract");
        if (reference_keys.empty())
        {
            reference_keys = keys;
        }
        else
        {
            require(keys == reference_keys, "all engine locales must use the same key tree");
        }

        for (const auto& key : keys)
        {
            require(key.find("menu_scene") == std::string::npos, "engine locale must not contain menu scene copy");
            require(key.find("character_select_scene") == std::string::npos,
                    "engine locale must not contain character select copy");
            require(key.find("ui_test") == std::string::npos,
                "engine locale must not contain project demo UI copy");
            require(key.find("theme") == std::string::npos, "engine locale must not contain theme copy");
        }
    }

    return 0;
}
