#pragma once

#include "../../core/geometry/rect.h"
#include "../../core/geometry/vector2.h"

#include <variant>

namespace elysia::physics
{
struct AabbShape
{
    elysia::core::Rect local_rect{};
};

struct CircleShape
{
    elysia::core::Vector2 local_center{};
    float radius = 0.0f;
};

using ColliderShape = std::variant<AabbShape, CircleShape>;
}
