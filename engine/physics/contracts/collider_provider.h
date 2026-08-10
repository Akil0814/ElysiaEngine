#pragma once

#include "../collision/collider.h"

#include <span>

namespace elysia::physics
{
class ColliderProvider
{
public:
    virtual ~ColliderProvider() = default;

    [[nodiscard]] virtual std::span<const Collider> colliders() const noexcept = 0;
};
}
