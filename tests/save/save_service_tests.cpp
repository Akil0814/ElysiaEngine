#include "engine/save/save_service.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using elysia::tests::require;
using elysia::save::SaveClosePolicy;
using elysia::save::SaveCreateMode;
using elysia::save::SaveData;
using elysia::save::SaveError;
using elysia::save::SaveService;

void write(const std::filesystem::path& path,std::string_view text)
{
    std::ofstream output(path,std::ios::binary | std::ios::trunc);
    output << text;
}

std::string read(const std::filesystem::path& path)
{
    std::ifstream input(path,std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

bool has_corrupt_archive(
    const std::filesystem::path& directory,
    std::string_view save_name)
{
    const std::string prefix = std::string(save_name) + ".json.";
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        const std::string name = entry.path().filename().string();
        if (name.starts_with(prefix) && name.ends_with(".corrupt")) return true;
    }
    return false;
}

void test_save_data_contract()
{
    SaveData data;
    require(data.set("bool",true).value(),"new bool value must change SaveData");
    require(data.set("int",std::int64_t{42}).value(),"new int64 value must change SaveData");
    require(data.set("double",3.5).value(),"new double value must change SaveData");
    require(data.set("string",std::string{"moon"}).value(),"new string value must change SaveData");
    require(data.set("bools",std::vector<bool>{true,false}).value(),"bool array must be supported");
    require(data.set("ints",std::vector<std::int64_t>{1,2}).value(),"int64 array must be supported");
    require(data.set("doubles",std::vector<double>{1.25,2.5}).value(),"double array must be supported");
    require(data.set("strings",std::vector<std::string>{}).value(),"empty string array must be supported");
    require(!data.set("int",std::int64_t{42}).value(),"setting an equal value must not report a change");
    require(data.get<std::int64_t>("int") == 42,"exact int64 lookup must succeed");
    require(data.get<std::vector<std::string>>("strings")->empty(),"empty typed array must be readable");

    const auto mismatch = data.get<double>("int");
    require(!mismatch && mismatch.error().error == SaveError::TypeMismatch,
        "SaveData must reject implicit numeric conversion");
    const auto missing = data.get<std::string>("missing");
    require(!missing && missing.error().error == SaveError::KeyNotFound,
        "missing SaveData keys must be distinct from type mismatches");
    require(!data.set("",std::int64_t{1})
        && data.set("",std::int64_t{1}).error().error == SaveError::InvalidKey,
        "empty SaveData keys must be rejected");
    require(!data.set("bad",std::numeric_limits<double>::infinity()),
        "non-finite doubles must be rejected");
    require(!data.set("bad_array",std::vector<double>{1.0,std::nan("")}),
        "arrays containing non-finite doubles must be rejected");

    const auto prefixed = data.keys("do");
    require(prefixed == std::vector<std::string>({"double","doubles"}),
        "SaveData prefix keys must be deterministic and sorted");
    require(data.erase("bool") && !data.erase("bool"),
        "SaveData erase must report whether a key existed");
}

void test_service_and_persistence(const std::filesystem::path& directory)
{
    auto* service = SaveService::instance();
    service->shutdown();
    require(!service->create("slot_01")
        && service->create("slot_01").error().error == SaveError::NotInitialized,
        "SaveService operations must fail before initialization");
    require(service->initialize(directory).has_value(),
        "SaveService must initialize a save directory");
    require(!service->initialize(directory)
        && service->initialize(directory).error().error == SaveError::AlreadyInitialized,
        "SaveService must reject repeated initialization");

    for (std::string_view invalid : {
        "","slot.json","../slot","folder/slot","CON","COM1","name with space"})
    {
        const auto created = service->create(invalid);
        require(!created && created.error().error == SaveError::InvalidSaveName,
            "SaveService must reject unsafe or non-logical save names");
    }

    require(service->create("slot_01").has_value(),"new save must open as an empty document");
    require(service->is_open("slot_01"),"created save must be cached as open");
    require(service->is_dirty("slot_01").value(),"created save must start dirty");
    require(service->revision("slot_01") == 1,"created save must start at revision one");

    require(service->set("slot_01","player.level",std::int64_t{12}).has_value(),
        "service set must mutate an open save");
    require(service->set("slot_01","player.alive",true).has_value(),
        "service must store bool values");
    require(service->set("slot_01","player.ratio",0.75).has_value(),
        "service must store double values");
    require(service->set("slot_01","player.name",std::string{"Luna"}).has_value(),
        "service must store string values");
    require(service->set("slot_01","flags",std::vector<bool>{true,false}).has_value(),
        "service must store bool arrays");
    require(service->set("slot_01","scores",std::vector<std::int64_t>{4,8}).has_value(),
        "service must store int64 arrays");
    require(service->set("slot_01","weights",std::vector<double>{1.0,2.0}).has_value(),
        "service must store double arrays");
    require(service->set("slot_01","progress.stages",std::vector<std::string>{}).has_value(),
        "service must store empty string arrays");

    const auto before_equal_set = service->revision("slot_01").value();
    require(service->set("slot_01","player.level",std::int64_t{12}).has_value()
        && service->revision("slot_01") == before_equal_set,
        "setting an equal value must not advance the revision");
    require(service->get<std::int64_t>("slot_01","player.level") == 12,
        "service get must return exact typed values");
    require(service->contains("slot_01","player.level").value(),
        "service contains must inspect an open save");
    require(service->keys("slot_01","player.")
            == std::vector<std::string>({
                "player.alive","player.level","player.name","player.ratio"}),
        "service prefix keys must be deterministic and sorted");
    require(!service->get<double>("slot_01","player.level")
        && service->get<double>("slot_01","player.level").error().error
            == SaveError::TypeMismatch,
        "service get must surface exact type failures");

    require(service->create("slot_02").has_value(),"service must cache multiple saves");
    require(service->set("slot_02","player.level",std::int64_t{3}).has_value(),
        "second cached save must be independently mutable");
    require(service->get<std::int64_t>("slot_01","player.level") == 12
        && service->get<std::int64_t>("slot_02","player.level") == 3,
        "cached saves must not share SaveData");
    SaveData replacement;
    require(replacement.set("player.level",std::int64_t{5}).has_value(),
        "replacement fixture must be valid SaveData");
    require(service->replace("slot_02",replacement).has_value()
        && service->get<std::int64_t>("slot_02","player.level") == 5,
        "replace must atomically replace one cached document");
    const auto replacement_revision = service->revision("slot_02").value();
    require(service->replace("slot_02",replacement).has_value()
        && service->revision("slot_02") == replacement_revision,
        "replacing with equal data must not advance the revision");
    auto detached_snapshot = service->snapshot("slot_02").value();
    require(detached_snapshot.set("player.level",std::int64_t{7}).has_value()
        && service->get<std::int64_t>("slot_02","player.level") == 5,
        "snapshot must return a detached copy rather than mutable cached state");
    require(service->erase("slot_02","player.level").value()
        && !service->contains("slot_02","player.level").value(),
        "service erase must update the cached document");

    require(service->commit("slot_01").has_value(),"dirty save must commit");
    require(!service->is_dirty("slot_01").value(),"successful commit must clear dirty state");
    const auto primary = directory / "slot_01.json";
    require(std::filesystem::is_regular_file(primary),"commit must append the JSON extension internally");
    const std::string encoded = read(primary);
    require(encoded.find("\"types\"") != std::string::npos
        && encoded.find("\"string_array\"") != std::string::npos,
        "serialized save must include strict type metadata");

    require(service->close("slot_01").has_value(),"clean save must close");
    const auto reopened = service->open("slot_01");
    require(reopened && !reopened->recovered,"valid primary save must reopen without recovery");
    require(service->get<std::vector<std::string>>(
            "slot_01","progress.stages")->empty(),
        "empty arrays must preserve their exact type after a disk round-trip");
    require(service->get<bool>("slot_01","player.alive").value()
        && service->get<double>("slot_01","player.ratio") == 0.75
        && service->get<std::string>("slot_01","player.name") == "Luna"
        && service->get<std::vector<bool>>("slot_01","flags")
            == std::vector<bool>({true,false})
        && service->get<std::vector<std::int64_t>>("slot_01","scores")
            == std::vector<std::int64_t>({4,8})
        && service->get<std::vector<double>>("slot_01","weights")
            == std::vector<double>({1.0,2.0}),
        "all supported scalar and array types must survive a disk round-trip");

    require(!service->close("slot_02")
        && service->close("slot_02").error().error == SaveError::DirtySave,
        "default close policy must reject dirty saves");
    require(service->close("slot_02",SaveClosePolicy::DiscardChanges).has_value(),
        "explicit discard policy must close dirty saves");

    const auto names = service->list_save_names();
    require(names && *names == std::vector<std::string>({"slot_01"}),
        "save listing must be sorted and include committed saves");

    require(service->close("slot_01").has_value(),"reopened clean save must close");
    require(service->exists("slot_01").value(),"exists must find a committed closed save");
    require(!service->create("slot_01")
        && service->create("slot_01").error().error == SaveError::AlreadyExists,
        "create must not overwrite an existing save by default");
    require(service->create("slot_01",SaveCreateMode::OverwriteExisting).has_value(),
        "explicit overwrite must create an empty dirty document");
    require(service->snapshot("slot_01")->keys().empty(),
        "overwrite mode must start from empty SaveData");
    require(service->close("slot_01",SaveClosePolicy::DiscardChanges).has_value(),
        "discarding an uncommitted overwrite must preserve the disk save");
    require(service->open("slot_01").has_value()
        && service->get<std::int64_t>("slot_01","player.level") == 12,
        "discarded overwrite must not mutate the disk save");
    require(!service->remove("slot_01")
        && service->remove("slot_01").error().error == SaveError::AlreadyOpen,
        "remove must reject an open save");
    require(service->close("slot_01").has_value(),"save must close before recovery tests");
}

void test_recovery_and_failures(const std::filesystem::path& directory)
{
    auto* service = SaveService::instance();
    const auto primary = directory / "slot_01.json";
    const auto temporary = directory / "slot_01.json.tmp";
    const auto backup = directory / "slot_01.json.bak";
    std::filesystem::copy_file(
        primary,backup,std::filesystem::copy_options::overwrite_existing);
    write(primary,"{broken");

    const auto recovered_backup = service->open("slot_01");
    require(recovered_backup && recovered_backup->recovered
        && service->get<std::int64_t>("slot_01","player.level") == 12,
        "a valid backup must recover a corrupt primary");
    require(has_corrupt_archive(directory,"slot_01"),
        "corrupt primary saves must be archived");
    require(service->close("slot_01").has_value(),"recovered save must be clean");

    std::filesystem::copy_file(
        primary,temporary,std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(primary);
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
    const auto recovered_temporary = service->open("slot_01");
    require(recovered_temporary && recovered_temporary->recovered,
        "an orphaned valid temporary file must be promoted");
    require(service->close("slot_01").has_value(),"temporary recovery must produce a clean save");

    const std::string valid = read(primary);
    write(primary,R"({"format_version":99,"types":{},"values":{}})");
    write(backup,valid);
    const auto future = service->open("slot_01");
    require(!future && future.error().error == SaveError::UnsupportedFormatVersion,
        "future primary format must fail without backup downgrade");
    require(read(primary).find("99") != std::string::npos,
        "future-version primary must remain untouched");

    write(primary,R"({"format_version":1,"types":{"x":"string_array"},"values":{}})");
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
    const auto mismatched_tables = service->open("slot_01");
    require(!mismatched_tables
        && mismatched_tables.error().error == SaveError::InvalidDocument,
        "types and values key mismatch must be rejected");

    write(primary,R"({"format_version":1,"format_version":1,"types":{},"values":{}})");
    const auto duplicate = service->open("slot_01");
    require(!duplicate && duplicate.error().error == SaveError::InvalidDocument,
        "duplicate JSON properties must be rejected");

    write(primary,valid);
    require(service->open("slot_01").has_value(),"valid save must reopen for write-failure test");
    require(service->set("slot_01","player.level",std::int64_t{13}).has_value(),
        "write-failure fixture must dirty the save");
    if (std::filesystem::exists(backup)) std::filesystem::remove_all(backup);
    std::filesystem::create_directories(backup);
    write(backup / "blocker","prevent backup replacement");
    const auto blocked_commit = service->commit("slot_01");
    require(!blocked_commit && blocked_commit.error().error == SaveError::IoFailure
        && service->is_dirty("slot_01").value(),
        "commit failure must remain visible and retain dirty state");
    std::filesystem::remove_all(backup);
    require(service->close("slot_01",SaveClosePolicy::DiscardChanges).has_value(),
        "failed commit fixture must be discardable");

    write(temporary,valid);
    write(backup,valid);
    require(service->remove("slot_01").has_value(),
        "remove must delete a closed save and its recovery files");
    require(!std::filesystem::exists(primary)
        && !std::filesystem::exists(temporary)
        && !std::filesystem::exists(backup),
        "remove must delete primary, temporary, and backup files");
    require(!service->remove("slot_01")
        && service->remove("slot_01").error().error == SaveError::NotFound,
        "removing an absent save must return NotFound");

    service->shutdown();
    service->shutdown();
    require(!service->is_initialized(),"SaveService shutdown must be idempotent");

    const auto blocked_directory = directory / "not_a_directory";
    write(blocked_directory,"block directory creation");
    const auto blocked_initialization = service->initialize(blocked_directory);
    require(!blocked_initialization
        && blocked_initialization.error().error == SaveError::IoFailure,
        "initialization must fail when the configured save path is a file");
    require(!service->is_initialized(),
        "failed initialization must not publish a partially initialized service");
    service->shutdown();
}
}

int main()
{
    const auto directory = std::filesystem::temp_directory_path()
        / "elysia_save_service_tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    test_save_data_contract();
    test_service_and_persistence(directory);
    test_recovery_and_failures(directory);

    std::filesystem::remove_all(directory);
}
