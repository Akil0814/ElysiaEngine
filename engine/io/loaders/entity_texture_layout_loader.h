#pragma once
#include "asset_config_types.h"
#include <filesystem>
namespace elysia::io { class EntityTextureLayoutLoader { public: bool load(const std::filesystem::path&, EntityTextureLayout&) const; }; }
