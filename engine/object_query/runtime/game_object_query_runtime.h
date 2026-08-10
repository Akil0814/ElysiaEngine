#pragma once

#include "../game_object_query_types.h"

namespace elysia::object_query
{
class IGameObjectQueryRuntime
{
public:
    virtual ~IGameObjectQueryRuntime() = default;

    virtual void visit_game_objects(const GameObjectVisitor& visitor) const = 0;
};
}
