#include "startup_preload_loader.h"

#include "../builtin/resources/builtin_asset_keys.h"
#include "../io/json/json_duplicate_key_checker.h"
#include "../resources/texture/surface_loader.h"
#include "../resources/texture/texture_loader.h"
#include "../io/path/path_manager.h"
#include "../resources/pipeline/resource_key_builder.h"

#include <unordered_set>
#include <utility>

namespace elysia::bootstrap
{
void StartupPreloadLoader::set_manifest_path(const std::filesystem::path& preload_manifest_path)
{
    reset();
    _manifest_path = preload_manifest_path;
}

void StartupPreloadLoader::reset()
{
    release_textures();
    _manifest_path.clear();
    _manifest_loader.reset();
    _project_textures.clear();
}

void StartupPreloadLoader::release_textures() noexcept
{
    _texture_cache.clear();
    _renderer = nullptr;
    _is_loaded = false;
}

std::expected<void,BootstrapFailure>
StartupPreloadLoader::load(SDL_Renderer* renderer)
{
    if (_is_loaded && renderer == _renderer)
        return {};

    if (!renderer)
        return std::unexpected(BootstrapFailure{
            "Bootstrapper phase2 failed: renderer is null."
        });

    if (_manifest_path.empty())
        return std::unexpected(BootstrapFailure{
            "Bootstrapper phase2 failed: preload manifest path is not prepared."
        });

    if (auto manifest_result = load_manifest(); !manifest_result)
        return manifest_result;

    BootstrapTextureCache prepared_cache;
    if (auto texture_result = load_textures(renderer,prepared_cache);
        !texture_result)
        return texture_result;

    _texture_cache = std::move(prepared_cache);
    _renderer = renderer;
    _is_loaded = true;
    return {};
}

SDL_Texture* StartupPreloadLoader::find_texture(
    std::string_view key) const noexcept
{
    return _texture_cache.find(key);
}

std::expected<void,BootstrapFailure> StartupPreloadLoader::load_manifest()
{
    _project_textures.clear();
    if (elysia::io::has_duplicate_json_object_key(_manifest_path))
        return std::unexpected(BootstrapFailure{
            "Load preload manifest failed: duplicate JSON object key: "
                + _manifest_path.string()
        });

    const elysia::io::JsonReadResult result = _manifest_loader.open_file(_manifest_path);
    if (!result.success)
        return std::unexpected(BootstrapFailure{
            "Load preload manifest failed: " + result.error
        });

    const elysia::io::json& root = _manifest_loader.root();
    if (!root.is_object() || root.size() != 1 || !root.contains("textures")
        || !root.at("textures").is_array())
        return std::unexpected(BootstrapFailure{
            "Load preload manifest failed: root must contain only a textures array."
        });

    std::unordered_set<std::string> keys;
    std::vector<TextureEntry> parsed_entries;
    const elysia::io::json& textures = root.at("textures");
    parsed_entries.reserve(textures.size());

    for (const elysia::io::json& node : textures)
    {
        if (!node.is_object() || node.size() != 2
            || !node.contains("key") || !node.at("key").is_string()
            || !node.contains("file") || !node.at("file").is_string())
            return std::unexpected(BootstrapFailure{
                "Load preload manifest failed: every texture must contain "
                "string key and file fields."
            });

        TextureEntry entry;
        entry.key = node.at("key").get<std::string>();
        entry.file = node.at("file").get<std::string>();

        std::string key_error;
        if (!elysia::resources::ResourceKeyBuilder::validate_key(entry.key,key_error))
            return std::unexpected(BootstrapFailure{
                "Load preload manifest failed: invalid texture key: " + key_error
            });
        if (entry.key == elysia::builtin::asset_keys::ElysiaWhiteTexture)
            return std::unexpected(BootstrapFailure{
                "Load preload manifest failed: project texture uses the "
                "engine-reserved key: " + entry.key
            });
        if (!keys.insert(entry.key).second)
            return std::unexpected(BootstrapFailure{
                "Load preload manifest failed: duplicate texture key: " + entry.key
            });
        if (entry.file.empty() || entry.file.is_absolute())
            return std::unexpected(BootstrapFailure{
                "Load preload manifest failed: texture file must be a non-empty "
                "relative path: " + entry.key
            });

        entry.file = entry.file.lexically_normal();
        for (const std::filesystem::path& component : entry.file)
        {
            if (component == "..")
                return std::unexpected(BootstrapFailure{
                    "Load preload manifest failed: texture file escapes the "
                    "preload root: " + entry.key
                });
        }

        parsed_entries.push_back(std::move(entry));
    }

    _project_textures = std::move(parsed_entries);
    return {};
}

std::expected<void,BootstrapFailure> StartupPreloadLoader::load_textures(
    SDL_Renderer* renderer,
    BootstrapTextureCache& destination
)
{
    for (const TextureEntry& entry : _project_textures)
    {
        const std::filesystem::path file =
            elysia::io::PathManager::instance()->preload() / entry.file;
        if (auto result = load_texture(
                renderer,
                entry.key,
                file,
                destination);
            !result)
            return result;
    }

    return {};
}

std::expected<void,BootstrapFailure> StartupPreloadLoader::load_texture(
    SDL_Renderer* renderer,
    std::string_view key,
    const std::filesystem::path& file,
    BootstrapTextureCache& destination)
{
    elysia::resources::SurfaceLoadRequest surface_request;
    surface_request._asset_key = std::string(key);
    surface_request._frame_path = file;
    surface_request._frame_index = 0;

    elysia::resources::SurfaceLoader surface_loader;
    elysia::resources::SurfaceLoadResult surface_result =
        surface_loader.load_surface(surface_request);
    if (!surface_result._success)
        return std::unexpected(BootstrapFailure{
            "Load required preload texture surface failed: key="
                + std::string(key) + ", file=" + file.string()
        });

    elysia::resources::TextureLoader texture_loader;
    elysia::resources::TextureLoadResult texture_result =
        texture_loader.load_texture(renderer,surface_result);
    if (!texture_result._success)
        return std::unexpected(BootstrapFailure{
            "Create required preload texture failed: key="
                + std::string(key) + ", file=" + file.string()
        });

    if (!destination.store(std::string(key),std::move(texture_result._texture)))
        return std::unexpected(BootstrapFailure{
            "Load preload texture failed: duplicate or invalid cache key: "
                + std::string(key)
        });

    return {};
}

}
