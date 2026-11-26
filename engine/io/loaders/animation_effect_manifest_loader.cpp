#include "animation_effect_manifest_loader.h"

#include "../json/json_loader.h"
#include "../../effects/particle/particle_effect.h"

#include <array>
#include <cmath>
#include <limits>
#include <ranges>
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
namespace
{
template<class T>
bool read_number(const json& node, T& value)
{
    if (!node.is_number())
        return false;
    const double number = node.get<double>();
    if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<T>::max())
        return false;
    value = static_cast<T>(number);
    return true;
}

bool read_unsigned(const json& node, std::size_t& value)
{
    if (!node.is_number_integer() || (!node.is_number_unsigned() && node.get<std::int64_t>() < 0))
        return false;
    const auto number = node.get<std::uint64_t>();
    if (number > std::numeric_limits<std::size_t>::max())
        return false;
    value = static_cast<std::size_t>(number);
    return true;
}

template<std::size_t Count>
bool read_tuple(const json& node, std::array<float, Count>& values)
{
    if (!node.is_array() || node.size() != Count)
        return false;
    for (std::size_t i = 0; i < Count; ++i)
        if (!read_number(node[i], values[i]))
            return false;
    return true;
}

bool read_vector(const json& node, elysia::core::Vector2& value)
{
    std::array<float, 2> components;
    if (!read_tuple(node, components))
        return false;
    value = {components[0], components[1]};
    return true;
}

bool read_color(const json& node, elysia::core::Color& value)
{
    if (!node.is_array() || node.size() != 4)
        return false;
    std::array<std::uint8_t, 4> channels;
    for (std::size_t i = 0; i < channels.size(); ++i)
    {
        std::size_t channel;
        if (!read_unsigned(node[i], channel) || channel > 255)
            return false;
        channels[i] = static_cast<std::uint8_t>(channel);
    }
    value = {channels[0], channels[1], channels[2], channels[3]};
    return true;
}

bool read_range(const json& node, elysia::effects::ParticleRange& value)
{
    return node.is_object() && node.size() == 2 && node.contains("min") && node.contains("max")
        && read_number(node.at("min"), value.min) && read_number(node.at("max"), value.max);
}

std::expected<elysia::resources::ParticleEffectBuildRequest, ManifestLoadFailure> read_particle(
    const json& node, const std::filesystem::path& path, const std::string& pointer)
{
    using namespace elysia::effects;
    ParticleEffectDefinition definition;
    const auto fail = [&](ManifestLoadError error, std::string message, std::string_view field = {}) {
        auto field_pointer = pointer;
        if (!field.empty())
            field_pointer += "/" + json_pointer_escape(std::string(field));
        return std::unexpected(make_manifest_load_failure(error, std::move(message),
            "particle-effect", definition.effect_key, path, path, std::move(field_pointer)));
    };
    if (!node.is_object())
        return fail(ManifestLoadError::InvalidSchema, "Particle effect entry must be an object.");

    for (const auto& [field, destination] : {std::pair{"key", &definition.effect_key},
        std::pair{"texture_key", &definition.texture_key}})
    {
        if (!node.contains(field))
            return fail(ManifestLoadError::MissingField, "Missing required particle effect field.", field);
        if (!node.at(field).is_string())
            return fail(ManifestLoadError::InvalidField, "Particle effect resource key must be a string.", field);
        *destination = node.at(field).get<std::string>();
        if (auto valid = elysia::resources::ResourceKeyBuilder::validate_key(*destination); !valid)
            return fail(ManifestLoadError::InvalidResourceKey, valid.error().message, field);
    }

    for (const auto& [field, value] : node.items())
    {
        bool valid = true;
        if (field == "key" || field == "texture_key")
            continue;
        if (field == "max_particles") valid = read_unsigned(value, definition.max_particles);
        else if (field == "burst_count") valid = read_unsigned(value, definition.burst_count);
        else if (field == "emission_rate") valid = read_number(value, definition.emission_rate);
        else if (field == "emission_duration_seconds") valid = read_number(value, definition.emission_duration_seconds.emplace());
        else if (field == "velocity_min") valid = read_vector(value, definition.velocity_min);
        else if (field == "velocity_max") valid = read_vector(value, definition.velocity_max);
        else if (field == "acceleration") valid = read_vector(value, definition.acceleration);
        else if (field == "start_size") valid = read_vector(value, definition.start_size);
        else if (field == "end_size") valid = read_vector(value, definition.end_size);
        else if (field == "start_color") valid = read_color(value, definition.start_color);
        else if (field == "end_color") valid = read_color(value, definition.end_color);
        else if (field == "lifetime_seconds") valid = read_range(value, definition.lifetime_seconds);
        else if (field == "angle_degrees") valid = read_range(value, definition.angle_degrees);
        else if (field == "angular_velocity_degrees") valid = read_range(value, definition.angular_velocity_degrees);
        else if (field == "source_rect")
        {
            std::array<float, 4> rect;
            valid = read_tuple(value, rect);
            if (valid)
                definition.source_rect.emplace(rect[0], rect[1], rect[2], rect[3]);
        }
        else if (field == "blend")
        {
            if (value == "alpha") definition.blend = ParticleBlendMode::Alpha;
            else if (value == "add") definition.blend = ParticleBlendMode::Add;
            else valid = false;
        }
        else if (field == "layer")
        {
            using elysia::core::DepthLayer;
            constexpr std::array layers{
                std::pair{"background", DepthLayer::Background}, std::pair{"terrain", DepthLayer::Terrain},
                std::pair{"effect_back", DepthLayer::EffectBack}, std::pair{"item", DepthLayer::Item},
                std::pair{"character", DepthLayer::Character}, std::pair{"effect_front", DepthLayer::EffectFront},
                std::pair{"foreground", DepthLayer::Foreground}};
            const auto match = std::ranges::find_if(layers, [&](const auto& layer) { return value == layer.first; });
            valid = match != layers.end();
            if (valid)
                definition.layer = match->second;
        }
        else
            return fail(ManifestLoadError::UnknownField, "Unknown particle effect field: " + field, field);
        if (!valid)
            return fail(ManifestLoadError::InvalidField, "Invalid particle effect field: " + field, field);
    }
    if (auto valid = ParticleEffect::validate_parameters(definition); !valid)
        return fail(ManifestLoadError::InvalidValue, valid.error().diagnostic.message, valid.error().field);

    auto origin = elysia::resources::make_resource_origin(
        path, pointer, {}, "particle_effects", {}, definition.effect_key);
    return elysia::resources::ParticleEffectBuildRequest{std::move(definition), std::move(origin)};
}
}

std::expected<AnimationEffectManifest,ManifestLoadFailure>
AnimationEffectManifestLoader::load(const std::filesystem::path& manifest_path) const
{
    const auto fail = [&manifest_path](ManifestLoadError error,std::string message,
        std::string key = {},std::string pointer = {},
        std::source_location origin = std::source_location::current())
        -> std::expected<AnimationEffectManifest,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),"effect",std::move(key),manifest_path,
            manifest_path,std::move(pointer),origin));
    };
    JsonLoader loader;
    const auto read = loader.open_file(manifest_path);
    if (!read)
        return std::unexpected(manifest_failure_from_json(
            read.error(),"effect-manifest","Load effect manifest failed: "));
    if (!loader.root().is_object() || !loader.root().contains("effects")
        || !loader.root().at("effects").is_array())
        return fail(ManifestLoadError::MissingField,
            "Load effect manifest failed: effects is missing or not an array.",{},"/effects");
    for (auto field = loader.root().begin(); field != loader.root().end(); ++field)
        if (field.key() != "effects" && field.key() != "particle_effects")
            return fail(ManifestLoadError::UnknownField,
                "Load effect manifest failed: unknown root field.",{},
                "/" + json_pointer_escape(field.key()));

    AnimationEffectManifest manifest;
    std::size_t index = 0;
    for (const json& node : loader.root().at("effects"))
    {
        const std::string pointer = "/effects/" + std::to_string(index++);
        if (!node.is_object() || !node.contains("key") || !node.at("key").is_string()
            || !node.contains("animation_key") || !node.at("animation_key").is_string())
            return fail(ManifestLoadError::InvalidSchema,
                "Load effect manifest failed: invalid effect entry.",{},pointer);
        AnimationEffectManifestEntry entry;
        entry.key = node.at("key").get<std::string>();
        entry.animation_key = node.at("animation_key").get<std::string>();
        if (node.contains("default_width"))
        {
            if (!node.at("default_width").is_number())
                return fail(ManifestLoadError::InvalidField,
                    "Load effect manifest failed: default_width is invalid.",entry.key,pointer + "/default_width");
            entry.default_width = node.at("default_width").get<float>();
        }
        if (node.contains("default_height"))
        {
            if (!node.at("default_height").is_number())
                return fail(ManifestLoadError::InvalidField,
                    "Load effect manifest failed: default_height is invalid.",entry.key,pointer + "/default_height");
            entry.default_height = node.at("default_height").get<float>();
        }
        if (node.contains("default_angle_degrees"))
        {
            if (!node.at("default_angle_degrees").is_number())
                return fail(ManifestLoadError::InvalidField,
                    "Load effect manifest failed: default_angle_degrees is invalid.",entry.key,pointer + "/default_angle_degrees");
            entry.default_angle_degrees = node.at("default_angle_degrees").get<double>();
        }
        if (entry.default_width < 0 || entry.default_height < 0
            || ((entry.default_width == 0) != (entry.default_height == 0)))
            return fail(ManifestLoadError::InvalidValue,
                "Load effect manifest failed: default size must provide positive width and height.",entry.key,pointer);
        for (auto field = node.begin();field != node.end();++field)
            if (field.key() != "key" && field.key() != "animation_key"
                && field.key() != "default_width" && field.key() != "default_height"
                && field.key() != "default_angle_degrees")
                return fail(ManifestLoadError::UnknownField,
                    "Load effect manifest failed: unknown field: " + field.key(),entry.key,
                    pointer + "/" + json_pointer_escape(field.key()));
        if (entry.key.empty() || entry.animation_key.empty())
            return fail(ManifestLoadError::InvalidValue,
                "Load effect manifest failed: effect values must not be empty.",entry.key,pointer);
        auto effect_key_result = elysia::resources::ResourceKeyBuilder::validate_key(entry.key);
        auto animation_key_result = elysia::resources::ResourceKeyBuilder::validate_key(entry.animation_key);
        if (!effect_key_result || !animation_key_result)
        {
            const auto& key_failure = !effect_key_result
                ? effect_key_result.error() : animation_key_result.error();
            return fail(ManifestLoadError::InvalidResourceKey,
                "Load effect manifest failed: " + key_failure.message,entry.key,
                pointer + (!effect_key_result ? "/key" : "/animation_key"),
                key_failure.origin);
        }
        entry.origin = elysia::resources::make_resource_origin(
            manifest_path,pointer,{},"effects",{},entry.key);
        manifest.effects.push_back(std::move(entry));
    }
    if (auto particles = loader.root().find("particle_effects"); particles != loader.root().end())
    {
        if (!particles->is_array())
            return fail(ManifestLoadError::InvalidSchema, "particle_effects must be an array.", {}, "/particle_effects");
        for (std::size_t i = 0; i < particles->size(); ++i)
        {
            auto request = read_particle((*particles)[i], manifest_path, "/particle_effects/" + std::to_string(i));
            if (!request)
                return std::unexpected(std::move(request.error()));
            manifest.particle_effects.push_back(std::move(*request));
        }
    }
    return manifest;
}
}
