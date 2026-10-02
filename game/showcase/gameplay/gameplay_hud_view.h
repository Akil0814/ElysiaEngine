#pragma once
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/ui_bar.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/physics/physics_world.h"
#include <functional>
namespace example::showcase::gameplay
{
struct GameplayHudData
{
    int health=0,maximum=0;
    std::size_t enemies=0;
    elysia::physics::PhysicsStepStats physics{};
    std::uint64_t dropped=0;
    bool bound=false,attacking=false,alive=false;
};
struct GameplayActions { std::function<void()> pause,step,reset,world_ui,speech,back; };
class GameplayHudView final
{
public:
    void build(elysia::ui::UiWindow&,const char* title,const char* controls);
    void build_controls(elysia::ui::UiWindow&,GameplayActions);
    void update(const GameplayHudData&);
    void defeated();
    void clear() noexcept { _health_bar=nullptr;_stats_label=nullptr;_status_label=nullptr; }
private:
    elysia::ui::UiBar* _health_bar=nullptr;
    elysia::ui::UiLabel* _stats_label=nullptr;
    elysia::ui::UiLabel* _status_label=nullptr;
};
}
