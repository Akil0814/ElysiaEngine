#pragma once
#include "game/showcase/scenarios/scenario_gallery_scene.h"
#include "game/navigation/showcase_scene_keys.h"
namespace example::scene {
class PhysicsGalleryScene final : public ScenarioGalleryScene {
public: PhysicsGalleryScene():ScenarioGalleryScene(example::scene_keys::PhysicsGallery,false) {}
};
}
