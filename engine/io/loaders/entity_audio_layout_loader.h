#pragma once
#include "asset_config_types.h"
#include <filesystem>
namespace elysia::io { class EntityAudioLayoutLoader { public: bool load(const std::filesystem::path&, EntityAudioLayout&) const; }; }
