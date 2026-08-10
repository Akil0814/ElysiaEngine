#include "strict_json.h"

#include <fstream>
#include <set>
#include <vector>

namespace elysia::io
{
std::expected<json,std::string> load_strict_json(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open()) return std::unexpected("JSON open failed: " + path.string());
    std::vector<std::set<std::string>> object_keys;
    std::string duplicate;
    auto callback = [&](int,json::parse_event_t event,json& parsed)
    {
        if (event == json::parse_event_t::object_start) object_keys.emplace_back();
        else if (event == json::parse_event_t::key && !object_keys.empty())
        {
            const std::string key = parsed.get<std::string>();
            if (!object_keys.back().insert(key).second && duplicate.empty()) duplicate = key;
        }
        else if (event == json::parse_event_t::object_end && !object_keys.empty()) object_keys.pop_back();
        return true;
    };
    try
    {
        json result = json::parse(input,callback,true,true);
        if (!duplicate.empty()) return std::unexpected("Duplicate JSON property '" + duplicate + "' in " + path.string());
        return result;
    }
    catch (const std::exception& exception)
    {
        return std::unexpected("JSON parse failed: " + path.string() + ": " + exception.what());
    }
}
}
