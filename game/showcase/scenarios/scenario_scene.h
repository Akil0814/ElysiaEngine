#pragma once
#include "game/showcase/shared/physics_inspector.h"
#include "engine/scene/scene.h"
#include "game/showcase/scenarios/scenario_enter_payload.h"
#include "game/showcase/scenarios/scenario_hud_view.h"
#include "engine/tools/debug_draw.h"
namespace example::scene
{
class ScenarioScene : public elysia::scene::Scene
{
public:
    ScenarioScene(elysia::scene::SceneKey key,bool gameplay);
    ~ScenarioScene() override;
protected:
    void on_enter(const elysia::scene::ScenePayload&) override;
    void on_exit() override;
    void on_reset() override;
    void on_before_update(double) override;
    void on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>&) override;
private:
    void run(); void pause_case(); void step(); void restart(); void back();
    void release_global_state() noexcept;
    void clear_presentation() noexcept;
    elysia::scene::SceneKey _key;
    bool _gameplay;
    ScenarioEnterPayload _entry;
    std::unique_ptr<example::showcase::scenarios::ShowcaseScenario> _scenario;
    elysia::core::GameObject* _presentation=nullptr;
    elysia::ui::UiWindow* _window=nullptr;
    example::showcase::scenarios::ScenarioHudView _hud;
    example::showcase::PhysicsInspector _inspector;
    bool _captured=false, _previous_enabled=false;
    elysia::tools::DebugDrawCategory _previous_categories=elysia::tools::DebugDrawCategory::All;
};
class GameplayVerificationScene final : public ScenarioScene
{
public: GameplayVerificationScene();
};
}
