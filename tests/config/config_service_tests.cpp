#include "engine/config/content/config_load_pipeline.h"
#include "engine/config/content/config_manifest_loader.h"
#include "engine/config/config_service.h"
#include "engine/io/path/path_manager.h"
#include "tests/support/test_assertions.h"

#include <filesystem>
#include <fstream>

namespace
{
using elysia::tests::require;
void write(const std::filesystem::path& path,std::string_view text)
{ std::ofstream(path,std::ios::trunc) << text; }
std::string json_path(const std::filesystem::path& path) { return path.generic_string(); }
void manifest(const std::filesystem::path& dir,std::string configs)
{ write(dir/"manifest.json","{\"schema_version\":1,\"configs\":"+std::move(configs)+"}"); }
std::string entry(std::string_view key,const std::filesystem::path& path)
{ return "\""+std::string(key)+"\":\""+json_path(path)+"\""; }
}

int main()
{
    require(elysia::io::PathManager::instance()->init(),"PathManager must initialize for config path resolution");
    const auto dir = std::filesystem::temp_directory_path() / "elysia_config_service_tests";
    std::filesystem::remove_all(dir); std::filesystem::create_directories(dir);
    const auto game = dir/"game.json"; const auto list = dir/"list.json";
    const auto integer = dir/"integer.json"; const auto text = dir/"text.json";
    const auto boolean = dir/"boolean.json"; const auto decimal = dir/"decimal.json";
    write(game,R"({"count":7,"ratio":2.5,"enabled":true,"name":"hero","position":{"x":1,"y":2},"bounds":{"x":1,"y":2,"width":3,"height":4},"ints":[1,2,3],"points":[{"x":1,"y":2},{"x":3,"y":4}],"empty":[],"empty_object":{}})");
    write(list,R"([10,20])"); write(integer,"42"); write(text,R"("root")");
    write(boolean,"true"); write(decimal,"3.5");
    manifest(dir,"{"+entry("game",game)+","+entry("list",list)+","+entry("integer",integer)
        +","+entry("text",text)+","+entry("boolean",boolean)+","+entry("decimal",decimal)+"}");

    elysia::config::ConfigLoadPipeline pipeline;
    manifest(dir,"{}");
    auto empty_snapshot = pipeline.load(dir/"manifest.json");
    require(empty_snapshot.has_value(),"an empty config manifest must build an empty snapshot");
    manifest(dir,"{"+entry("game",game)+","+entry("list",list)+","+entry("integer",integer)
        +","+entry("text",text)+","+entry("boolean",boolean)+","+entry("decimal",decimal)+"}");
    auto snapshot = pipeline.load(dir/"manifest.json");
    require(snapshot.has_value(),"ConfigLoadPipeline must build arbitrary non-null JSON documents");
    auto* service = elysia::config::ConfigService::instance(); service->shutdown(); service->publish(*snapshot);
    require(service->contains("game") && service->contains("game.position")
        && service->contains("game.ints.1") && service->contains("list.0"),
        "root values, objects and array elements must all be indexed");
    require(service->get_int("integer").value()==42 && service->get_int("list.1").value()==20,
        "integer and array roots must be accessible from namespace keys");
    require(service->get_string("text").value()=="root" && service->get_bool("boolean").value()
        && service->get_double("decimal").value()==3.5,"scalar root getters must work");
    require(service->get_int("game.count").value()==7 && service->get_double("game.count").value()==7.0,
        "integer and double conversion rules must remain available");
    require(service->get_vector2("game.position").value()==elysia::core::Vector2{1,2}
        && service->get_rect("game.bounds").value()==elysia::core::Rect{1,2,3,4},
        "strict geometry getters must remain available");
    require(service->get_int_array("game.ints").value().size()==3
        && service->get_vector2_array("game.points").value().size()==2
        && service->get_string_array("game.empty").value().empty(),"typed arrays including empty arrays must work");
    require(!service->get_int("game.ratio") && !service->get_rect("game.position")
        && !service->get_int("game.missing"),"access failures must still return expected errors");

    write(dir/"bad.json",R"({"bad-name":1})"); manifest(dir,"{"+entry("bad",dir/"bad.json")+"}");
    require(!pipeline.load(dir/"manifest.json"),"illegal object key component must fail snapshot construction");
    require(service->get_int("game.count").value()==7,"failed pipeline load must not replace published snapshot");
    write(dir/"overlap.json",R"({"value":1})"); write(dir/"base.json",R"({"rules":{"value":2}})");
    manifest(dir,"{"+entry("game",dir/"base.json")+","+entry("game.rules",dir/"overlap.json")+"}");
    auto duplicate = pipeline.load(dir/"manifest.json");
    require(!duplicate && duplicate.error().error==elysia::config::ConfigLoadError::DuplicateKey
        && !duplicate.error().first.config_path.empty() && !duplicate.error().second.config_path.empty(),
        "overlapping namespaces must report both origins");
    write(dir/"null.json","null"); manifest(dir,"{"+entry("nulls",dir/"null.json")+"}");
    require(!pipeline.load(dir/"manifest.json"),"null document root must fail snapshot construction");
    write(dir/"nested_null.json",R"({"value":null})"); manifest(dir,"{"+entry("nulls",dir/"nested_null.json")+"}");
    require(!pipeline.load(dir/"manifest.json"),"nested null must fail snapshot construction");
    write(dir/"duplicate.json",R"({"value":1,"value":2})"); manifest(dir,"{"+entry("duplicates",dir/"duplicate.json")+"}");
    auto duplicate_property = pipeline.load(dir/"manifest.json");
    require(!duplicate_property && duplicate_property.error().error==elysia::config::ConfigLoadError::DuplicateKey
        && !duplicate_property.error().first.config_path.empty() && !duplicate_property.error().second.config_path.empty(),
        "duplicate JSON properties must retain first and second origins");

    write(dir/"invalid_manifest.json",R"({"schema_version":1,"configs":{},"extra":true})");
    require(!elysia::config::ConfigManifestLoader{}.load(dir/"invalid_manifest.json"),
        "config manifest must reject unknown fields");
    write(dir/"legacy_manifest.json",R"({"schema_version":1,"documents":{}})");
    require(!elysia::config::ConfigManifestLoader{}.load(dir/"legacy_manifest.json"),
        "config manifest must retain the configs mapping field");
    write(dir/"invalid_namespace.json","{\"schema_version\":1,\"configs\":{"+entry("bad-name",game)+"}}");
    require(!elysia::config::ConfigManifestLoader{}.load(dir/"invalid_namespace.json"),
        "config manifest must reject invalid namespace components");
    write(dir/"missing_document.json",R"({"schema_version":1,"configs":{"missing":"missing.json"}})");
    require(!elysia::config::ConfigManifestLoader{}.load(dir/"missing_document.json"),
        "config manifest must reject missing document paths");
    write(dir/"duplicate_namespace.json","{\"schema_version\":1,\"configs\":{"+entry("game",game)+","+entry("game",list)+"}}");
    auto duplicate_namespace = elysia::config::ConfigManifestLoader{}.load(dir/"duplicate_namespace.json");
    require(!duplicate_namespace && duplicate_namespace.error().error==elysia::config::ConfigLoadError::DuplicateKey,
        "config manifest must reject duplicate namespaces");

    service->shutdown();
    require(!service->is_initialized() && !service->get_int("game.count"),
        "shutdown must clear the published snapshot");
    std::filesystem::remove_all(dir);
}
