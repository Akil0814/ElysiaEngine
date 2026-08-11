#pragma once

#include "../../../engine/scene/routing/scene_route.h"

namespace example::scene
{
struct DemoScenePayload
{
    elysia::scene::SceneRoute return_route{};
};
}
