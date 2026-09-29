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
    int width = 1280,height = 720,x = 17,y = 29;
    bool fullscreen = false;
    WindowOperationResult call(std::string operation)
    {
        calls.push_back(operation);
        if (std::find(fail_calls.begin(),fail_calls.end(),static_cast<int>(calls.size())) != fail_calls.end())
            return std::unexpected(elysia::core::make_failure_diagnostic("injected " + operation));
        return {};
    }
    ApplicationWindowOperations operations()
    {
        return {
            .set_fullscreen = [&](bool value) { auto result = call(value ? "fullscreen" : "windowed"); if (result) fullscreen = value; return result; },
            .set_size = [&](int w,int h) { auto result = call("size"); if (result) { width=w; height=h; } return result; },
            .set_position = [&](int px,int py) { auto result = call("position"); if (result) { x=px; y=py; } return result; }
        };
    }
};
struct Handler final : elysia::config::IUserConfigChangeHandler
{
    Recorder recorder;
    std::expected<void,elysia::config::UserConfigFailure> apply_window_settings(const elysia::config::WindowSettings& settings) override
    {
        auto result = apply_window_settings_transactional(settings,{{WindowMode::Windowed,{1280,720}},17,29},recorder.operations());
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
    service->shutdown();
    std::filesystem::remove(path);
}
