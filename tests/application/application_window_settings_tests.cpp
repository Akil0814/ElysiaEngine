#define SDL_MAIN_HANDLED
#include "engine/application/presentation/application_window_settings.h"
#include "engine/config/user_config_service.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>
#include <string>
#include <filesystem>

namespace
{
using elysia::tests::require;
using namespace elysia::application::detail;
using elysia::config::WindowMode;
struct Recorder
{
    std::vector<std::string> calls;
    std::vector<int> fail_calls;
    std::vector<std::string> queries,fail_queries;
    int width = 1280,height = 720,x = 17,y = 29;
    bool fullscreen = false;
    WindowOperationResult call(std::string operation)
    {
        calls.push_back(operation);
        if (std::find(fail_calls.begin(),fail_calls.end(),static_cast<int>(calls.size())) != fail_calls.end())
            return std::unexpected(elysia::core::make_failure_diagnostic("injected " + operation));
        return {};
    }
    WindowOperationResult query(std::string name)
    {
        queries.push_back(name);
        if (std::find(fail_queries.begin(),fail_queries.end(),name)!=fail_queries.end())
            return std::unexpected(elysia::core::make_failure_diagnostic("snapshot " + name));
        return {};
    }
    ApplicationWindowOperations operations()
    {
        return {
            .set_fullscreen = [&](bool value) { auto result = call(value ? "fullscreen" : "windowed"); if (result) fullscreen = value; return result; },
            .set_size = [&](int w,int h) { auto result = call("size"); if (result) { width=w; height=h; } return result; },
            .set_position = [&](int px,int py) { auto result = call("position"); if (result) { x=px; y=py; } return result; },
            .get_mode = [&]() -> std::expected<WindowMode,elysia::core::FailureDiagnostic> {
                if (auto result = query("mode"); !result) return std::unexpected(result.error());
                return fullscreen ? WindowMode::BorderlessFullscreen : WindowMode::Windowed;
            },
            .get_size = [&]() -> std::expected<elysia::config::WindowSize,elysia::core::FailureDiagnostic> {
                if (auto result = query("size"); !result) return std::unexpected(result.error());
                return elysia::config::WindowSize{width,height};
            },
            .get_position = [&]() -> std::expected<WindowPosition,elysia::core::FailureDiagnostic> {
                if (auto result = query("position"); !result) return std::unexpected(result.error());
                return WindowPosition{x,y};
            }
        };
    }
};
struct Handler final : elysia::config::IUserConfigChangeHandler
{
    Recorder recorder;
    std::expected<void,elysia::config::UserConfigFailure> apply_window_settings(const elysia::config::WindowSettings& settings) override
    {
        const auto operations = recorder.operations();
        auto result = validate_window_settings(settings,operations);
        if (result)
        {
            auto previous = capture_window_snapshot({WindowMode::Windowed,{1280,720}},operations);
            if (!previous) result = std::unexpected(previous.error());
            else result = apply_window_settings_transactional(settings,*previous,operations);
        }
        if (!result) return std::unexpected(elysia::config::UserConfigFailure{
            elysia::config::UserConfigError::RuntimeApplyFailed,"window_settings",result.error().message,result.error()});
        return {};
    }
    std::expected<void,elysia::config::UserConfigFailure> apply_master_volume(int) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure> apply_music_volume(int) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure> apply_sound_volume(int) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure> apply_language(std::string_view) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure> apply_target_fps(double) override { return {}; }
};
}
int main()
{
    const elysia::config::WindowSettings requested{WindowMode::Windowed,{1600,900}};
    const ApplicationWindowSnapshot previous{{WindowMode::Windowed,{1280,720}},17,29};
    Recorder normal;
    Recorder invalid;
    require(!apply_window_settings_transactional({WindowMode::Windowed,{0,720}},previous,invalid.operations())
        && invalid.calls.empty(),"invalid settings must not mutate or restore the physical window");
    require(apply_window_settings(requested,normal.operations()) && normal.calls == std::vector<std::string>{"windowed","size","position"},"windowed operations must be ordered");
    Recorder borderless;
    require(apply_window_settings({WindowMode::BorderlessFullscreen,{1280,720}},borderless.operations()) && borderless.calls == std::vector<std::string>{"fullscreen"},"fullscreen preserves saved windowed size");
    for (int failing : {1,2,3})
    {
        Recorder recorder;
        recorder.fail_calls = {failing};
        const auto result = apply_window_settings_transactional(requested,previous,recorder.operations());
        require(!result && recorder.calls.size() == failing + 3,"every failure must stop applying and force all rollback operations");
        require(recorder.width==1280 && recorder.height==720 && recorder.x==17 && recorder.y==29 && !recorder.fullscreen,"physical state must be restored");
    }
    Recorder multiple;
    Recorder failed_fullscreen;
    failed_fullscreen.fail_calls = {1};
    require(!apply_window_settings_transactional({WindowMode::BorderlessFullscreen,{1280,720}},previous,failed_fullscreen.operations())
        && failed_fullscreen.calls == std::vector<std::string>{"fullscreen","windowed","size","position"}
        && !failed_fullscreen.fullscreen,"fullscreen failure must restore actual windowed state");
    multiple.fail_calls = {3,4,5};
    const auto failed = apply_window_settings_transactional(requested,previous,multiple.operations());
    require(!failed && failed.error().message=="injected position" && failed.error().entries.size()==2
        && multiple.calls.back()=="position","preserve first failure and all rollback failures, continuing restoration");
    Recorder old_fullscreen;
    old_fullscreen.fail_calls = {2};
    require(!apply_window_settings_transactional(requested,{{WindowMode::BorderlessFullscreen,{1280,720}},17,29},old_fullscreen.operations())
        && old_fullscreen.fullscreen,"rollback must restore fullscreen after remembered window size and position");
    for (const char* failing : {"position","mode","size"})
    {
        Recorder recorder;
        recorder.fail_queries = {failing};
        const auto snapshot = capture_window_snapshot(previous.settings,recorder.operations());
        require(!snapshot && snapshot.error().message==std::string("snapshot ")+failing && recorder.calls.empty(),
            "snapshot failure must retain its operation and never mutate the window");
    }
    Recorder physical;
    physical.width=900; physical.height=600; physical.x=41; physical.y=43;
    const auto snapshot = capture_window_snapshot(previous.settings,physical.operations());
    require(snapshot && snapshot->settings.windowed_size==elysia::config::WindowSize{900,600}
        && snapshot->x==41 && snapshot->y==43,"snapshot must capture physical windowed size and position");
    physical.fullscreen=true; physical.fail_queries={"size"}; physical.queries.clear();
    const auto full_snapshot = capture_window_snapshot(previous.settings,physical.operations());
    require(full_snapshot && full_snapshot->settings.mode==WindowMode::BorderlessFullscreen
        && full_snapshot->settings.windowed_size==previous.settings.windowed_size
        && physical.queries==std::vector<std::string>{"position","mode"},
        "fullscreen snapshots must preserve remembered windowed size");
    Recorder startup_windowed;
    const auto windowed_start = apply_startup_window_settings(previous.settings,startup_windowed.operations());
    require(windowed_start && !*windowed_start && startup_windowed.calls.empty(),"normal windowed startup must preserve existing creation behavior");
    const elysia::config::WindowSettings fullscreen{WindowMode::BorderlessFullscreen,{1280,720}};
    Recorder startup_fullscreen;
    const auto full_start = apply_startup_window_settings(fullscreen,startup_fullscreen.operations());
    require(full_start && !*full_start && startup_fullscreen.calls==std::vector<std::string>{"fullscreen"},
        "successful fullscreen startup must not restore the window");
    Recorder startup_fallback;
    startup_fallback.fail_calls={1};
    const auto fallback = apply_startup_window_settings(fullscreen,startup_fallback.operations());
    require(fallback && *fallback && (**fallback).message=="injected fullscreen"
        && startup_fallback.calls==std::vector<std::string>{"fullscreen","windowed","size","position"}
        && startup_fallback.x==SDL_WINDOWPOS_CENTERED && startup_fallback.y==SDL_WINDOWPOS_CENTERED,
        "successful fallback must report the original fullscreen failure for logging");
    for (int failing : {2,3,4})
    {
        Recorder recorder;
        recorder.fail_calls={1,failing};
        const auto result = apply_startup_window_settings(fullscreen,recorder.operations());
        require(!result && result.error().message=="injected fullscreen" && result.error().entries.size()==1
            && recorder.calls.size()==4,"failed startup restoration must retain both errors and attempt all steps");
    }
    Recorder startup_multiple;
    startup_multiple.fail_calls={1,2,3,4};
    const auto startup_failed = apply_startup_window_settings(fullscreen,startup_multiple.operations());
    require(!startup_failed && startup_failed.error().entries.size()==3 && startup_multiple.calls.size()==4,
        "startup must retain every failed restoration operation");
    Recorder restore_multiple;
    restore_multiple.fail_calls={1,2,3,4};
    const auto restore_failed = restore_window_snapshot({fullscreen,17,29},restore_multiple.operations());
    require(!restore_failed && restore_failed.error().message=="injected windowed"
        && restore_failed.error().entries.size()==3 && restore_multiple.calls.back()=="fullscreen",
        "restore helper must try reenabling old fullscreen even after earlier failures");
    require(SDL_Init(SDL_INIT_VIDEO),"SDL window adapter fixture");
    SDL_Window* window = SDL_CreateWindow("window settings adapter",320,240,SDL_WINDOW_HIDDEN);
    require(window,"adapter window");
    const auto real_snapshot = capture_window_snapshot(previous.settings,make_sdl_window_operations(window));
    require(real_snapshot && real_snapshot->settings.windowed_size==elysia::config::WindowSize{320,240},
        "SDL adapter must capture actual size rather than configured size");
    const auto null_snapshot = capture_window_snapshot(previous.settings,make_sdl_window_operations(nullptr));
    require(!null_snapshot && null_snapshot.error().message.find("SDL_GetWindowPosition")!=std::string::npos,
        "SDL snapshot failure must capture the failing operation immediately");
    SDL_DestroyWindow(window); SDL_Quit();
    Handler handler;
    auto* service = elysia::config::UserConfigService::instance();
    const auto path = std::filesystem::temp_directory_path() / "elysia_window_failure_config.json";
    std::filesystem::remove(path);
    elysia::config::UserConfigData defaults;
    defaults.language = "en";
    defaults.window = previous.settings;
    require(service->initialize(defaults,path),"settings fixture must initialize");
    auto& config = service->user_config();
    const auto committed = config.window_settings();
    service->register_user_config_change_handler(handler);
    handler.recorder.fail_calls = {2};
    const auto rejected = config.set_window_settings(requested);
    require(!rejected && config.window_settings()==committed,"failed physical change must not commit settings or report Applied");
    handler.recorder.calls.clear(); handler.recorder.fail_calls.clear(); handler.recorder.fail_queries={"size"};
    const auto snapshot_rejected = config.set_window_settings(requested);
    require(!snapshot_rejected && config.window_settings()==committed && handler.recorder.calls.empty(),
        "snapshot failure must reject the config change before any physical mutation");
    service->shutdown();
    std::filesystem::remove(path);
}
