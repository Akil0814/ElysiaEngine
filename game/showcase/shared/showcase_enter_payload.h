#pragma once

#include "engine/scene/routing/scene_route.h"

namespace example::scene
{
struct ShowcaseEnterPayload
{
    elysia::scene::SceneRoute return_route{};
};
}
