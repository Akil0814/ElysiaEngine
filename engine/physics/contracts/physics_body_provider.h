#pragma once

#include "../body/physics_body.h"

namespace elysia::physics
{
class PhysicsBodyProvider
{
public:
    virtual ~PhysicsBodyProvider() = default;

    [[nodiscard]] virtual PhysicsBody* physics_body() noexcept = 0;
    [[nodiscard]] virtual const PhysicsBody* physics_body() const noexcept = 0;
};
}
