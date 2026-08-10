#pragma once

#include "../scene/routing/scene_route.h"

namespace elysia::realm
{
struct ElysiaScenePayload
{
    elysia::scene::SceneRoute return_route{};
};
}
