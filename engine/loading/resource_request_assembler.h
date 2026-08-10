#pragma once

#include <span>
#include <string>

namespace elysia::loading
{
struct ContentManifestResult;
class ResourceLoadPlan;

class ResourceRequestAssembler
{
public:
	bool assemble(
		const ContentManifestResult& config_result,
		std::span<const int> project_font_point_sizes,
		ResourceLoadPlan& out_plan) const;
	[[nodiscard]] const std::string& error_message() const { return _error_message; }

private:
	mutable std::string _error_message;
};

}
