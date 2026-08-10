#pragma once

#include "../../scene/routing/scene_route.h"

namespace elysia::builtin
{
struct SettingsScenePayload
{
    elysia::scene::SceneRoute return_route{};
};
}
