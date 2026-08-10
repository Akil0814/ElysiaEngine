#pragma once

#include "../config/user_config_data.h"
#include "../io/loaders/asset_config_types.h"

#include <filesystem>
#include <string>

namespace elysia::bootstrap
{
struct BootstrapFailure
{
    std::string message;
};

struct RuntimeSettings
{
    std::string window_title = "Moonline";
    elysia::config::UserConfigData user;
};

struct BootstrapOutput
{
    RuntimeSettings runtime_settings;
    elysia::io::ContentRegistry content_registry;
    std::filesystem::path i18n_manifest_path;
    std::string warning;
};
}
