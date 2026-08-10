#include "resource_request_assembler.h"

#include "content_manifest_pipeline.h"
#include "resource_load_plan.h"
#include "resource_load_plan_validator.h"
#include "../io/path/path_manager.h"
#include "../resources/pipeline/resource_request_builder.h"
#include "../tools/logger.h"

namespace elysia::loading
{
namespace
{
bool append_module_animations(
	const elysia::io::EntityContentModule& module,
	elysia::resources::ResourceRequestBuilder& builder,
	ResourceLoadPlan& plan)
{
	for (const auto& entry : module.animation_entries)
		if (!builder.append_entity_animation_requests(module.key_namespace, entry,
			plan.atlas_build_requests(), plan.animation_build_requests())) return false;
	return true;
}

bool append_module_effects(
	const elysia::io::EntityContentModule& module,
	elysia::resources::ResourceRequestBuilder& builder,
	ResourceLoadPlan& plan)
{
	for (const auto& entry : module.effect_entries)
		if (!builder.append_entity_effect_requests(module.key_namespace, entry,
			plan.animation_build_requests(), plan.animation_effect_build_requests())) return false;
	return true;
}
}

bool ResourceRequestAssembler::assemble(
	const ContentManifestResult& config,
	std::span<const int> project_font_point_sizes,
	ResourceLoadPlan& plan) const
{
	plan.clear();
	_error_message.clear();
	elysia::resources::ResourceRequestBuilder builder;
	const auto textures_root = elysia::io::PathManager::instance()->textures();
	const auto fail = [this, &plan](std::string message)
	{
		_error_message = std::move(message);
		ELYSIA_LOG_ERROR("resource", _error_message);
		plan.clear();
		return false;
	};

	// Phase 1: core animations.
	if (!builder.append_animation_manifest_requests(config.animation_manifest, textures_root,
		plan.atlas_build_requests(), plan.animation_build_requests()))
		return fail("Resource request assembly failed while building core animations.");

	// Phase 2: every additional module's animation resources.
	for (const auto& [name, module] : config.additional_modules)
		if (!append_module_animations(module, builder, plan))
			return fail("Resource request assembly failed while building module animations: " + name);

	// Phase 3: effects only bind animations already declared above.
	if (!builder.append_animation_effect_manifest_requests(
		config.animation_effect_manifest, plan.animation_effect_build_requests()))
		return fail("Resource request assembly failed while building core effects.");
	for (const auto& [name, module] : config.additional_modules)
		if (!append_module_effects(module, builder, plan))
			return fail("Resource request assembly failed while building module effects: " + name);

	// Phase 4: textures.
	if (!builder.append_texture_manifest_requests(config.texture_manifest, textures_root, plan.texture_requests()))
		return fail("Resource request assembly failed while building core textures.");
	for (const auto& [name, module] : config.additional_modules)
		for (const auto& entry : module.texture_entries)
			if (!builder.append_entity_texture_requests(module.key_namespace, entry, plan.texture_requests()))
				return fail("Resource request assembly failed while building module textures: " + name);

	// Phase 5: fonts and audio.
	if (!builder.append_font_requests(
			config.font_manifest,
			project_font_point_sizes,
			plan.font_requests())
		|| !builder.append_audio_requests(config.audio_manifest, plan.sound_requests(), plan.music_requests()))
		return fail("Resource request assembly failed while building core font/audio resources.");
	for (const auto& [name, module] : config.additional_modules)
		for (const auto& entry : module.audio_entries)
			if (!builder.append_entity_audio_requests(module.key_namespace, entry, plan.sound_requests()))
				return fail("Resource request assembly failed while building module audio: " + name);

	ResourceLoadPlanValidationError validation_error;
	if (!ResourceLoadPlanValidator{}.validate(plan, validation_error))
		return fail(validation_error.describe());
	return true;
}
}
