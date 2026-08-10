#pragma once

#include "../resources/resource_origin.h"

#include <string>
#include <expected>

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
	[[nodiscard]] std::expected<void,ResourceLoadPlanValidationError> validate(
		const ResourceLoadPlan& plan) const;
};
}
