#pragma once
#include "game/showcase/scenarios/scenario_gallery_scene.h"
#include "game/navigation/showcase_scene_keys.h"
namespace example::scene {
class GameplayGalleryScene final : public ScenarioGalleryScene {
public: GameplayGalleryScene():ScenarioGalleryScene(example::scene_keys::GameplayGallery,true) {}
};
}
