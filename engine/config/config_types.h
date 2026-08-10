#pragma once

#include <string>

namespace elysia::config
{
struct ConfigOrigin
{
    std::string config_path;
    std::string json_pointer;
    std::string key_namespace;
    std::string full_key;

    [[nodiscard]] std::string describe() const;
};

enum class ConfigLoadError { OpenFailed, InvalidSchema, InvalidKey, InvalidValue, DuplicateKey };
struct ConfigLoadFailure
{
    ConfigLoadError error = ConfigLoadError::InvalidSchema;
    std::string message;
    ConfigOrigin first;
    ConfigOrigin second;
};

enum class ConfigAccessError { NotInitialized, MissingKey, TypeMismatch, InvalidValue };
struct ConfigAccessFailure
{
    ConfigAccessError error = ConfigAccessError::MissingKey;
    std::string key;
    std::string expected_type;
    std::string actual_type;
    ConfigOrigin origin;
    std::string message;
};
}
