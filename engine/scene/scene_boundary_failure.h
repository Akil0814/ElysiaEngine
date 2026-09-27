#pragma once

#include "routing/scene_key.h"

#include <stdexcept>
#include <string>

namespace elysia::scene
{
enum class SceneBoundary
{
    Enter,
    Exit,
    Reset,
    Attach,
    Detach,
    Input,
    Update,
    Render,
    ObjectRegistration,
    ObjectRemoval
};

class SceneBoundaryTagged
{
public:
    explicit SceneBoundaryTagged(SceneBoundary boundary) noexcept : _boundary(boundary) {}
    virtual ~SceneBoundaryTagged() = default;

    [[nodiscard]] SceneBoundary scene_boundary() const noexcept { return _boundary; }

private:
    SceneBoundary _boundary;
};

class SceneBoundaryLogicError final : public std::logic_error,
                                      public SceneBoundaryTagged
{
public:
    SceneBoundaryLogicError(SceneBoundary boundary, const std::string& message)
        : std::logic_error(message), SceneBoundaryTagged(boundary)
    {
    }
};

class SceneBoundaryRuntimeError final : public std::runtime_error,
                                        public SceneBoundaryTagged
{
public:
    SceneBoundaryRuntimeError(SceneBoundary boundary, const std::string& message)
        : std::runtime_error(message), SceneBoundaryTagged(boundary)
    {
    }
};

struct SceneBoundaryFailure
{
    SceneKey scene = SceneKeys::Invalid;
    SceneBoundary boundary = SceneBoundary::Update;
    std::string message;
};
} // namespace elysia::scene
