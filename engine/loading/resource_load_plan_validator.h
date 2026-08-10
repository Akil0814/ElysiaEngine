#pragma once

#include "../resources/resource_origin.h"

#include <string>

namespace elysia::loading
{
class ResourceLoadPlan;

struct ResourceLoadPlanValidationError
{
	std::string registry;
	std::string key;
	elysia::resources::ResourceOrigin first;
	elysia::resources::ResourceOrigin second;
	std::string message;
	bool duplicate = false;

	[[nodiscard]] std::string describe() const;
};

class ResourceLoadPlanValidator
{
public:
	[[nodiscard]] bool validate(
		const ResourceLoadPlan& plan,
		ResourceLoadPlanValidationError& error
	) const;
};
}
