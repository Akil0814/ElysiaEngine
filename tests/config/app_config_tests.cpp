#include "engine/bootstrap/app_config_loader.h"
#include "tests/support/test_assertions.h"

#include <filesystem>
#include <fstream>

namespace
{
using elysia::tests::require;
std::filesystem::path write(std::string_view name,std::string_view json)
{
    const auto dir = std::filesystem::temp_directory_path() / "elysia_app_config_tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::ofstream(path) << json;
    return path;
}
}

int main()
{
    elysia::bootstrap::AppConfigLoader loader;
    const auto valid = loader.load(write("valid.json",R"({
      "schema_version":2,
      "window":{"title":"Elysia Engine","mode":"borderless_fullscreen","windowed_size":{"width":1280,"height":720}},
      "render":{"fps":60,"vsync":true},
      "audio":{"master_volume":100,"music_volume":80,"sound_volume":70},
      "localization":{"language":"en"}
    })"));
    require(valid.has_value(),"valid AppConfig v2 must load");
    require(valid->window_title == "Elysia Engine"
        && valid->user_defaults.language == "en"
        && valid->user_defaults.window.mode
            == elysia::config::WindowMode::BorderlessFullscreen
        && valid->user_defaults.window.windowed_size
            == elysia::config::WindowSize{ 1280,720 },
        "AppConfig values must be retained");
    require(!loader.load(write("old.json",R"({"schema_version":1,"window":{"title":"x","width":1,"height":1,"fullscreen":false},"render":{"fps":1,"vsync":false},"audio":{"master_volume":0,"music_volume":0,"sound_volume":0},"localization":{"language":"en"}})")),"AppConfig v1 must be rejected");
    const auto duplicate = loader.load(write("duplicate.json",R"({"schema_version":2,"schema_version":2})"));
    require(!duplicate && duplicate.error().diagnostic.entries.size() == 1
        && duplicate.error().diagnostic.entries.front().declaration_pointer
            == "/schema_version",
        "duplicate AppConfig properties must retain their JSON pointer");
    const auto range = loader.load(write("range.json",R"({"schema_version":2,"window":{"title":"x","mode":"windowed","windowed_size":{"width":0,"height":1}},"render":{"fps":1,"vsync":false},"audio":{"master_volume":0,"music_volume":0,"sound_volume":0},"localization":{"language":"en"}})"));
    require(!range && range.error().diagnostic.entries.size() == 1
        && range.error().diagnostic.entries.front().declaration_pointer
            == "/window/windowed_size/width",
        "invalid AppConfig values must retain their precise JSON pointer");
    require(!loader.load(write("mode.json",R"({"schema_version":2,"window":{"title":"x","mode":"exclusive_fullscreen","windowed_size":{"width":1,"height":1}},"render":{"fps":1,"vsync":false},"audio":{"master_volume":0,"music_volume":0,"sound_volume":0},"localization":{"language":"en"}})")),"unknown AppConfig window modes must be rejected");
    const auto unknown = loader.load(write("unknown.json",R"({"schema_version":2,"window":{"title":"x","mode":"windowed","windowed_size":{"width":1,"height":1},"fullscreen":false},"render":{"fps":1,"vsync":false},"audio":{"master_volume":0,"music_volume":0,"sound_volume":0},"localization":{"language":"en"}})"));
    require(!unknown && unknown.error().diagnostic.entries.size() == 1
        && unknown.error().diagnostic.entries.front().declaration_pointer
            == "/window/fullscreen",
        "unknown AppConfig fields must retain their precise JSON pointer");
    std::filesystem::remove_all(std::filesystem::temp_directory_path() / "elysia_app_config_tests");
}
