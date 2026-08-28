#include "realm_scene_composition.h"

#include "elysia_intro_scene.h"
#include "elysia_realm_scene.h"
#include "realm_scene_keys.h"
#include "../../builtin/resources/builtin_resources.h"
#include "../../scene/routing/scene_key.h"
#include "../../scene/scene_manager.h"

#include <functional>

namespace elysia::realm::detail
{
void register_realm_scenes(
    elysia::scene::SceneManager& scene_manager,
    const elysia::builtin::BuiltinResources& builtin_resources)
{
    scene_manager.register_engine_scene<ElysiaIntroScene>(
        elysia::scene::SceneKeys::ElysiaRealm,
        std::cref(builtin_resources));
    scene_manager.register_engine_scene<ElysiaRealmScene>(
        SceneKeys::RealmContent,
        std::cref(builtin_resources));
}
}
