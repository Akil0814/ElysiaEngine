#pragma once
#include "engine/scene/scene.h"
#include "game/showcase/shared/showcase_frame.h"
namespace example::scene
{
class ScenarioGalleryScene : public elysia::scene::Scene
{
protected:
    ScenarioGalleryScene(elysia::scene::SceneKey key,bool gameplay):_key(key),_gameplay(gameplay) {}
    void on_enter(const elysia::scene::ScenePayload&) override;
    void on_exit() override;
    void on_reset() override;
    void on_before_update(double) override;
    void on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>&) override;
private:
    void build_ui(); void back();
    elysia::scene::SceneRoute route() const;
    elysia::scene::SceneKey _key;
    bool _gameplay;
    int _tier=0;
    bool _rebuild=false;
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiWindow* _window=nullptr;
    example::showcase::ShowcaseFrame _view;
};
}
