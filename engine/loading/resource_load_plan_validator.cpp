#include "resource_load_plan_validator.h"

#include "resource_load_plan.h"

#include <sstream>
#include <unordered_map>

namespace elysia::loading
{
namespace
{
template<typename Range, typename KeySelector>
bool validate_registry(
	const char* registry,
	const Range& requests,
	KeySelector select_key,
	ResourceLoadPlanValidationError& error)
{
	std::unordered_map<std::string, elysia::resources::ResourceOrigin> origins;
	for (const auto& request : requests)
	{
		const std::string& key = select_key(request);
		const auto [position, inserted] = origins.emplace(key, request.origin);
		if (!inserted)
		{
			error.registry = registry;
			error.key = key;
			error.first = position->second;
			error.second = request.origin;
			error.duplicate = true;
			return false;
		}
	}
	return true;
}
}

std::string ResourceLoadPlanValidationError::describe() const
{
	if (!duplicate) return message;
	std::ostringstream stream;
	stream << "Duplicate " << registry << " key: " << key
		<< "\n  first:  " << first.describe()
		<< "\n  second: " << second.describe();
	return stream.str();
}

bool ResourceLoadPlanValidator::validate(
	const ResourceLoadPlan& plan,
	ResourceLoadPlanValidationError& error) const
{
	error = {};
	if (!validate_registry("Atlas", plan.atlas_build_requests(),
		[](const auto& request) -> const std::string& { return request.atlas_key; }, error)
		|| !validate_registry("Animation", plan.animation_build_requests(),
			[](const auto& request) -> const std::string& { return request.animation_key; }, error)
		|| !validate_registry("Effect", plan.animation_effect_build_requests(),
			[](const auto& request) -> const std::string& { return request.effect_key; }, error)
		|| !validate_registry("Texture", plan.texture_requests(),
			[](const auto& request) -> const std::string& { return request.key; }, error)
		|| !validate_registry("Font", plan.font_requests(),
			[](const auto& request) -> const std::string& { return request.key; }, error)
		|| !validate_registry("Sound", plan.sound_requests(),
			[](const auto& request) -> const std::string& { return request.key; }, error)
		|| !validate_registry("Music", plan.music_requests(),
			[](const auto& request) -> const std::string& { return request.key; }, error))
		return false;

	std::unordered_map<std::string, bool> atlas_keys;
	for (const auto& request : plan.atlas_build_requests()) atlas_keys.emplace(request.atlas_key, true);
	for (const auto& request : plan.animation_build_requests())
	{
		if (!atlas_keys.contains(request.atlas_key))
		{
			error.message = "Animation references missing Atlas key: " + request.atlas_key
				+ "\n  source: " + request.origin.describe();
			return false;
		}
	}
	std::unordered_map<std::string, bool> animation_keys;
	for (const auto& request : plan.animation_build_requests()) animation_keys.emplace(request.animation_key, true);
	for (const auto& request : plan.animation_effect_build_requests())
	{
		if (!animation_keys.contains(request.animation_key))
		{
			error.message = "Effect references missing Animation key: " + request.animation_key
				+ "\n  source: " + request.origin.describe();
			return false;
		}
	}
	return true;
}
}
