#include "json_duplicate_key_checker.h"

#include "json_loader.h"

#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace elysia::io
{
bool has_duplicate_json_object_key(const std::filesystem::path& path)
{
	std::ifstream input(path);
	if (!input.is_open()) return false;
	std::unordered_map<int, std::unordered_set<std::string>> keys_by_depth;
	bool duplicate = false;
	const json::parser_callback_t callback = [&keys_by_depth, &duplicate](
		int depth, json::parse_event_t event, json& parsed)
	{
		if (event == json::parse_event_t::object_start) keys_by_depth[depth + 1].clear();
		else if (event == json::parse_event_t::key)
			duplicate = duplicate || !keys_by_depth[depth].insert(parsed.get<std::string>()).second;
		else if (event == json::parse_event_t::object_end) keys_by_depth.erase(depth + 1);
		return true;
	};
	try { [[maybe_unused]] const json parsed = json::parse(input, callback); }
	catch (...) { return false; }
	return duplicate;
}
}
