#include "testbed_scene_composition.h"

#include "scene/engine_feature_test_scene.h"
#include "scene/testbed_home_scene.h"
#include "scene/ui_test_scene.h"
#include "testbed_scene_keys.h"
#include "../scene/scene_manager.h"

namespace elysia::testbed
{
void register_testbed_scenes(elysia::scene::SceneManager& scene_manager)
{
    scene_manager.register_engine_scene<TestbedHomeScene>(SceneKeys::Home);
    scene_manager.register_engine_scene<UiTestScene>(SceneKeys::UiTest);
    scene_manager.register_engine_scene<EngineFeatureTestScene>(
        SceneKeys::EngineFeatureTest);
}
}
