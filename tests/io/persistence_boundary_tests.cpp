#include "engine/save/detail/save_store.h"
#include "engine/config/user/user_config_store.h"
#include "engine/config/user_config_service.h"
#include "engine/io/json/strict_json.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <new>
#include <utility>
#include <set>
#include <limits>
#include <iostream>
#include <sstream>

namespace
{
bool fail_large_allocation = false;
bool fail_next_allocation = false;
}

// Fail a parser's long string allocation, after the stream and small paths are prepared.
void* operator new(std::size_t size)
{
    if (std::exchange(fail_next_allocation,false)) throw std::bad_alloc{};
    if (fail_large_allocation && size >= 4096) throw std::bad_alloc{};
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory,std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory,std::size_t) noexcept { std::free(memory); }

namespace
{
using elysia::tests::require;
using namespace elysia;
using io::PersistenceRecovery;
using io::PersistenceFileState;
using io::detail::PersistenceOperations;

std::string read(const std::filesystem::path& path)
{
    std::ifstream input(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),{}};
}
void write(const std::filesystem::path& path,std::string_view content)
{
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    output << content;
}
bool has_reason(const core::FailureDiagnostic& diagnostic,std::string_view text)
{
    return std::ranges::any_of(diagnostic.entries,[&](const auto& entry) {
        return entry.reason.find(text) != std::string::npos;
    });
}

struct SaveFixture
{
    std::filesystem::path directory;
    std::filesystem::path primary() const { return directory/"slot.json"; }
    auto save(const PersistenceOperations& operations,int value) const
    {
        save::SaveData data;
        require(data.set("value",static_cast<std::int64_t>(value)).has_value(),"fixture value valid");
        return save::detail::SaveStore(directory,operations).save("slot",data);
    }
    auto load(const PersistenceOperations& operations) const
    {
        return save::detail::SaveStore(directory,operations).load("slot");
    }
};
struct ConfigFixture
{
    std::filesystem::path directory;
    std::filesystem::path primary() const { return directory/"settings.json"; }
    auto save(const PersistenceOperations& operations,int value) const
    {
        config::UserConfigData data;
        data.language = "en";
        data.audio.master_volume = value;
        return config::UserConfigStore(operations).save(primary(),data);
    }
    auto load(const PersistenceOperations& operations) const
    {
        config::UserConfigData data;
        data.language = "en";
        return config::UserConfigStore(operations).load(primary(),data);
    }
};

template<typename Fixture>
void test_store(const Fixture& fixture)
{
    std::filesystem::create_directories(fixture.directory);
    const auto primary = fixture.primary();
    const std::filesystem::path backup = primary.string()+".bak";
    const std::filesystem::path temporary = primary.string()+".tmp";
    const auto seed = [&]() {
        for (const auto& path : {primary,backup,temporary}) std::filesystem::remove(path);
        require(fixture.save({},10).has_value(),"baseline save succeeds");
    };
    const auto denial = std::make_error_code(std::errc::permission_denied);
    for (const auto stage : {"create-directory","open-temporary","write-temporary","flush-temporary",
        "close-temporary","verify-temporary","primary-status","backup-status","remove-backup","backup-primary","publish-primary"})
    {
        seed();
        const auto original = read(primary);
        write(backup,original);
        std::vector<std::string> calls;
        PersistenceOperations operations{[&](std::string_view operation,const auto&) {
            calls.emplace_back(operation);
            return operation == stage ? denial : std::error_code{};
        }};
        const auto result = fixture.save(operations,20);
        require(!result && result.error().persistence
            && result.error().persistence->stage == stage,"operation has exact structured failure stage");
        const auto& context = *result.error().persistence;
        require(context.primary.path == primary && context.backup.path == backup
            && context.temporary.path == temporary,"failure includes all artifact paths");
        require(read(primary) == original,"failed write restores or preserves original primary");
        require(context.primary.state == PersistenceFileState::Present,"reported state matches actual primary");
        require(context.recovery == (std::string_view(stage) == "publish-primary"
            ? PersistenceRecovery::Succeeded : PersistenceRecovery::NotRequired),"recovery status reflects actual work");
        require(result.error().diagnostic.message.find(stage) != std::string::npos
            && result.error().diagnostic.origin.line() != 0,"root diagnostic and source retained");
        require(std::find(calls.begin(),calls.end(),"publish-primary") == calls.end()
            || std::string_view(stage) == "publish-primary","failed preparation stops subsequent publication");
    }
    for (const bool query_failures : {false,true})
    {
        seed();
        const auto original = read(primary);
        std::vector<std::string> calls;
        PersistenceOperations operations{[&](std::string_view stage,const auto&) {
            calls.emplace_back(stage);
            if (stage == "publish-primary" || stage == "restore-primary"
                || (query_failures && (stage == "restore-backup-status" || stage == "restore-primary-status" || stage == "observe")))
                return denial;
            return std::error_code{};
        }};
        const auto result = fixture.save(operations,20);
        require(!result && result.error().diagnostic.message.find("publish-primary") != std::string::npos,
            "recovery failures never overwrite original publication failure");
        const auto& context = *result.error().persistence;
        require(context.recovery == (query_failures ? PersistenceRecovery::Unknown : PersistenceRecovery::Failed),
            "failed status and failed restoration are distinguished");
        require(has_reason(result.error().diagnostic,"restore-primary"),"restoration error preserved");
        if (query_failures)
        {
            require(has_reason(result.error().diagnostic,"restore-backup-status")
                && context.primary.state == PersistenceFileState::Unknown,"all query errors retained, unknown is not missing");
            require(std::find(calls.begin(),calls.end(),"restore-primary") == calls.end(),"uncertain queries forbid destructive restoration");
        }
        require(!std::filesystem::exists(primary) && read(backup) == original
            && std::filesystem::exists(temporary),"failed recovery preserves backup and temporary file");
        PersistenceOperations retry{[&](std::string_view stage,const auto&) {
            return stage == "publish-primary" ? denial : std::error_code{};
        }};
        const auto recovery_content = read(temporary);
        require(!fixture.save(retry,30) && read(primary) == recovery_content,
            "retry promotes newer valid temporary and restores it after publication fails");
        require(fixture.save({},30).has_value() && read(backup) == recovery_content,
            "successful retry backs up the recovered temporary content");
    }
    // Diagnostic failures after restoration must not discard earlier errors or restore twice.
    for (const bool restoration_failed : {false,true})
    {
        seed();
        const auto original = read(primary);
        int restore_calls = 0;
        bool observation_thrown = false;
        std::ostringstream logs;
        auto* previous = std::clog.rdbuf(logs.rdbuf());
        bool caught = false;
        try
        {
            (void)fixture.save(PersistenceOperations{[&](std::string_view stage,const auto&) -> std::error_code {
                if (stage == "publish-primary") return denial;
                if (stage == "restore-primary")
                {
                    ++restore_calls;
                    if (restoration_failed && restore_calls == 1) return std::make_error_code(std::errc::io_error);
                }
                if (stage == "observe" && !std::exchange(observation_thrown,true))
                    throw std::logic_error("observation-exception");
                return {};
            }},20);
        }
        catch (const std::logic_error& error) { caught = std::string_view(error.what()) == "observation-exception"; }
        catch (...) { std::clog.rdbuf(previous); throw; }
        std::clog.rdbuf(previous);
        const auto report = logs.str();
        require(caught && read(primary) == original && restore_calls == (restoration_failed ? 2 : 1),
            "exception preserves original data and retries only necessary restoration");
        require(report.find("publish-primary failed") != std::string::npos
            && report.find("observation-exception") != std::string::npos
            && report.find("reason=succeeded") != std::string::npos,
            "root failure and observation exception are logged even after successful recovery");
        if (restoration_failed)
            require(report.find("restore-primary failed") < report.find("observation-exception"),
                "first restoration error remains in chronological order");
    }
    seed();
    {
        std::ostringstream logs;
        auto* previous = std::clog.rdbuf(logs.rdbuf());
        int restores = 0;
        bool caught = false;
        try
        {
            (void)fixture.save(PersistenceOperations{[&](std::string_view stage,const auto&) -> std::error_code {
                if (stage == "publish-primary") return denial;
                if (stage == "restore-primary" && ++restores == 1)
                {
                    fail_next_allocation = true;
                    return std::make_error_code(std::errc::io_error);
                }
                return {};
            }},20);
        }
        catch (const std::bad_alloc&) { caught = true; }
        catch (...) { fail_next_allocation = false; std::clog.rdbuf(previous); throw; }
        fail_next_allocation = false;
        std::clog.rdbuf(previous);
        require(caught && restores == 2 && std::filesystem::exists(primary)
            && logs.str().find("restore-primary failed") != std::string::npos,
            "raw restoration failure survives report allocation failure and protected retry");
    }
    // A prior successful temporary write is the only recovery copy after first publication fails.
    for (const auto stage : {"open-temporary","write-temporary","flush-temporary","close-temporary",
            "verify-temporary","publish-primary"})
    {
        seed();
        const auto original = read(primary);
        std::filesystem::rename(primary,temporary);
        require(!fixture.save(PersistenceOperations{[&](std::string_view operation,const auto&) {
            return operation == stage ? denial : std::error_code{};
        }},20),"retry fault returns failure");
        require(read(primary) == original,"sole temporary is recovered before writing can damage it");
        require(fixture.load({}).has_value(),"old content remains loadable after retry failure");
        require(fixture.save({},30).has_value(),"subsequent save can commit new content");
    }
    for (const auto stage : {"primary-status","temporary-status","inspect-temporary","promote-temporary"})
    {
        seed();
        const auto original = read(primary);
        std::filesystem::rename(primary,temporary);
        require(!fixture.save(PersistenceOperations{[&](std::string_view operation,const auto&) {
            return operation == stage ? denial : std::error_code{};
        }},20) && read(temporary) == original && !std::filesystem::exists(primary),
            "preparation or promotion failure preserves sole temporary byte for byte");
    }
    seed();
    std::filesystem::remove(primary);
    write(temporary,"{invalid");
    require(fixture.save({},20).has_value(),"confirmed invalid temporary permits rewriting");
    seed();
    auto future = io::load_strict_json(primary).value();
    future[future.contains("schema_version") ? "schema_version" : "format_version"] = 999;
    std::filesystem::remove(primary);
    write(temporary,future.dump());
    const auto future_content = read(temporary);
    require(!fixture.save({},20) && read(temporary) == future_content,
        "future version temporary is protected from overwrite");
    seed();
    const auto older_backup = read(primary);
    require(fixture.save({},20).has_value(),"newer recovery content saved");
    const auto newer = read(primary);
    std::filesystem::rename(primary,temporary);
    require(fixture.save({},30).has_value() && read(backup) == newer && newer != older_backup,
        "temporary recovery takes precedence over older backup during normal rotation");
    seed();
    const auto close_failure = fixture.save(PersistenceOperations{[&](std::string_view stage,const auto&) {
        return stage == "write-temporary" || stage == "cleanup-close-temporary" ? denial : std::error_code{};
    }},20);
    require(!close_failure && close_failure.error().diagnostic.message.find("write-temporary") != std::string::npos
        && has_reason(close_failure.error().diagnostic,"cleanup-close-temporary"),
        "stream cleanup retains secondary close failure without replacing failed write");
    seed();
    const auto validation_original = read(primary);
    const auto invalid_temporary = fixture.save(PersistenceOperations{[&](std::string_view stage,const auto& path) {
        if (stage == "verify-temporary") write(path,"{broken");
        return std::error_code{};
    }},20);
    require(!invalid_temporary && invalid_temporary.error().persistence->stage == "verify-temporary"
        && read(primary) == validation_original,"actual verification failure stops primary replacement");
    for (const bool restore_throws : {false,true})
    {
        seed();
        const auto original = read(primary);
        bool attempted_restore = false;
        PersistenceOperations operations{[&](std::string_view stage,const auto&) -> std::error_code {
            if (stage == "publish-primary") throw std::bad_alloc{};
            if (stage == "restore-primary")
            {
                attempted_restore = true;
                if (restore_throws) throw std::logic_error("secondary cleanup exception");
            }
            return {};
        }};
        bool caught = false;
        try { (void)fixture.save(operations,20); }
        catch (const std::bad_alloc&) { caught = true; }
        require(caught && attempted_restore,"unexpected primary exception survives cleanup and cleanup exception");
        require(read(restore_throws ? backup : primary) == original,"necessary cleanup preserves original data");
    }
    seed();
    const auto before_unknown = read(primary);
    bool caught_unknown = false;
    try
    {
        (void)fixture.save(PersistenceOperations{[](std::string_view stage,const auto&) -> std::error_code {
            if (stage == "publish-primary") throw 42;
            return {};
        }},20);
    }
    catch (int value) { caught_unknown = value == 42; }
    require(caught_unknown && read(primary) == before_unknown,"non-standard exception propagates after restoration");
    for (const auto stage : {"status","read"})
    {
        seed();
        const auto original = read(primary);
        std::vector<std::string> calls;
        PersistenceOperations operations{[&](std::string_view operation,const auto&) {
            calls.emplace_back(operation);
            return operation == stage ? denial : std::error_code{};
        }};
        const auto result = fixture.load(operations);
        require(!result && read(primary) == original,"access failure never archives or overwrites primary");
        require(std::find(calls.begin(),calls.end(),"archive-corrupt") == calls.end()
            && std::find(calls.begin(),calls.end(),"open-temporary") == calls.end(),"access failure does not recover or rebuild");
    }
    seed();
    const auto original = read(primary);
    write(temporary,original);
    std::filesystem::remove(primary);
    PersistenceOperations blocked_promotion{[&](std::string_view stage,const auto&) {
        return stage == "promote-temporary" ? denial : std::error_code{};
    }};
    require(!fixture.load(blocked_promotion) && read(temporary) == original,"failed promotion preserves valid temporary source");
    const auto recovered = fixture.load({});
    require(recovered && recovered->recovered && recovered->warning
        && !recovered->warning->diagnostic.entries.empty(),"successful temporary recovery returns structured warning");
    write(primary,"{broken");
    PersistenceOperations blocked_archive{[&](std::string_view stage,const auto&) {
        return stage == "archive-corrupt" ? denial : std::error_code{};
    }};
    const auto archive = fixture.load(blocked_archive);
    require(!archive && read(primary) == "{broken" && archive.error().diagnostic.message.find("JSON parsing failed") != std::string::npos
        && has_reason(archive.error().diagnostic,"archive-corrupt"),
        "archive failure retains original corruption context and leaves source untouched");
    seed();
    const auto recovery_copy = read(primary);
    write(backup,recovery_copy);
    std::filesystem::remove(primary);
    const auto blocked_candidate = fixture.load(PersistenceOperations{[&](std::string_view stage,const auto& path) {
        return stage == "read" && path == backup ? denial : std::error_code{};
    }});
    require(!blocked_candidate && !std::filesystem::exists(primary) && read(backup) == recovery_copy,
        "candidate access failure never rebuilds defaults or overwrites recovery files");
}

void test_json_diagnostics(const std::filesystem::path& root)
{
    const auto config_directory = root/"json-config";
    std::filesystem::create_directories(config_directory);
    ConfigFixture configuration{config_directory};
    const auto first_run = configuration.load({});
    require(first_run && first_run->rebuilt && !first_run->warning,
        "first-run defaults remain normal success without a failure warning");
    require(configuration.save({},10).has_value(),"JSON config baseline");
    const auto config_path = configuration.primary();
    const auto valid = io::load_strict_json(config_path).value();
    for (const auto field : {"width","height"})
    {
        auto document = valid;
        document["window"]["windowed_size"][field] = std::numeric_limits<std::uint64_t>::max();
        write(config_path,document.dump());
        std::filesystem::remove(config_path.string()+".tmp");
        std::filesystem::remove(config_path.string()+".bak");
        const auto result = configuration.load({});
        require(result && result->rebuilt && result->warning
            && result->warning->diagnostic.entries.front().declaration_pointer == std::string("/window/windowed_size/")+field,
            "oversized unsigned fields have exact validation pointer");
    }
    auto future = valid;
    future["schema_version"] = std::numeric_limits<std::uint64_t>::max();
    const auto future_text = future.dump();
    write(config_path,future_text);
    require(!configuration.load({}) && read(config_path) == future_text,"oversized future schema is preserved without narrowing");
    const auto save_directory = root/"json-save";
    std::filesystem::create_directories(save_directory);
    SaveFixture saved{save_directory};
    write(saved.primary(),R"({"format_version":1,"types":{"a/b~c":"int64_array"},"values":{"a/b~c":[1,false]}})");
    const auto invalid = saved.load({});
    require(!invalid && has_reason(invalid.error().diagnostic,"int64"),"typed array element failure remains visible");
    require(std::ranges::any_of(invalid.error().diagnostic.entries,[](const auto& entry) {
        return entry.declaration_pointer == "/values/a~1b~0c/1";
    }),"save array failure includes escaped key and exact element pointer");
    const auto future_save = R"({"format_version":18446744073709551615})";
    write(saved.primary(),future_save);
    const auto unsupported = saved.load({});
    require(!unsupported && unsupported.error().error == save::SaveError::UnsupportedFormatVersion
        && read(saved.primary()) == future_save,"future save version is preserved even with an unknown document shape");
    const auto long_path = root/"allocation.json";
    write(long_path,"{\"value\":\""+std::string(16384,'x')+"\"}");
    bool caught = false;
    fail_large_allocation = true;
    try { (void)io::load_strict_json(long_path); }
    catch (const std::bad_alloc&) { caught = true; }
    catch (...) { fail_large_allocation = false; throw; }
    fail_large_allocation = false;
    require(caught,"parser allocation failure propagates instead of becoming ParseFailed");
    auto* service = config::UserConfigService::instance();
    config::UserConfigData defaults;
    defaults.language = "en";
    write(config_path,valid.dump());
    require(service->initialize(defaults,config_path).has_value(),"service state before failed reinitialization");
    const auto baseline = service->user_config().snapshot();
    bool initialize_caught = false;
    fail_large_allocation = true;
    try { (void)service->initialize(defaults,long_path); }
    catch (const std::bad_alloc&) { initialize_caught = true; }
    catch (...) { fail_large_allocation = false; throw; }
    fail_large_allocation = false;
    require(initialize_caught && service->is_initialized() && service->user_config().snapshot() == baseline,
        "failed initialization prepares state before publishing and preserves existing service");
    service->shutdown();
    service->shutdown();
    require(!service->is_initialized(),"shutdown remains nonthrowing and idempotent");
    const auto origin = std::source_location::current();
    const auto failure = save::make_save_failure(save::SaveError::InvalidValue,"slot","key","reason",origin);
    const auto report = core::format_failure_diagnostic(failure.diagnostic,"SAVE","save");
    require(failure.diagnostic.origin.line() == origin.line()
        && report.find("slot") != std::string::npos && report.find("reason") != std::string::npos,
        "domain diagnostic preserves source and context in report");
}
}

int main()
{
    const auto root = std::filesystem::temp_directory_path()/"elysia_persistence_boundary_tests";
    std::filesystem::remove_all(root);
    test_store(SaveFixture{root/"save"});
    test_store(ConfigFixture{root/"config"});
    test_json_diagnostics(root);
    std::filesystem::remove_all(root);
}
