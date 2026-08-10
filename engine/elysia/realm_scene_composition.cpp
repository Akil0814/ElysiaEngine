#include "realm_scene_composition.h"

#include "elysia_scene.h"
#include "../scene/routing/scene_key.h"
#include "../scene/scene_manager.h"

namespace elysia::realm
{
void register_realm_scene(elysia::scene::SceneManager& scene_manager)
{
    scene_manager.register_engine_scene<ElysiaScene>(
        elysia::scene::SceneKeys::ElysiaEasterEgg);
}
}
