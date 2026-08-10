#pragma once

#include "../../io/path/path_manager.h"

#include <expected>
#include <filesystem>
#include <cstddef>
#include <span>
#include <string_view>

namespace elysia::builtin
{
struct BuiltinAssetDescriptor
{
    std::string_view key;
    std::filesystem::path relative_path;
};

struct BuiltinLocaleDescriptor
{
    std::string_view locale;
    std::filesystem::path relative_path;
};

struct BuiltinAudioDescriptor
{
    std::string_view key;
    std::filesystem::path relative_path;
};

struct BuiltinAnimationDescriptor
{
    std::string_view key;
    std::string_view texture_key;
    int frame_width = 0;
    int frame_height = 0;
    std::size_t frame_count = 0;
    double fps = 0.0;
    bool loop = false;

    [[nodiscard]] bool has_expected_texture_dimensions(int width, int height) const noexcept
    {
        return frame_width > 0
            && frame_height > 0
            && frame_count > 0
            && width == frame_width * static_cast<int>(frame_count)
            && height == frame_height;
    }
};

enum class BuiltinAssetValidationErrorCode
{
    RootMissing,
    RequiredMarkerMissing,
    RequiredFileMissing
};

struct BuiltinAssetValidationError
{
    BuiltinAssetValidationErrorCode code;
    std::filesystem::path path;
};

class BuiltinAssetCatalog
{
public:
    explicit BuiltinAssetCatalog(std::filesystem::path project_root);
    explicit BuiltinAssetCatalog(const elysia::io::PathManager& path_manager);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] std::filesystem::path required_marker_path() const;
    [[nodiscard]] std::span<const BuiltinAssetDescriptor> fonts() const noexcept;
    [[nodiscard]] std::span<const BuiltinAssetDescriptor> textures() const noexcept;
    [[nodiscard]] std::span<const BuiltinLocaleDescriptor> locales() const noexcept;
    [[nodiscard]] std::span<const BuiltinAnimationDescriptor> animations() const noexcept;
    [[nodiscard]] std::span<const BuiltinAudioDescriptor> sounds() const noexcept;
    [[nodiscard]] std::span<const BuiltinAudioDescriptor> music() const noexcept;
    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& relative_path) const;
    [[nodiscard]] std::expected<void, BuiltinAssetValidationError>validate_required_files() const;

private:
    std::filesystem::path _root;
};
}
