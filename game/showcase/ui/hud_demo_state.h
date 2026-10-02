#pragma once
#include <algorithm>
#include <array>
namespace example::showcase::ui
{
struct HudDemoState
{
    static constexpr double kCooldown=3.0;
    double cooldown=0;
    int casts=0;
    std::array<int,2> item_counts{8,3};
    std::size_t selected_item=0;
    bool paused=false;
    void advance(double delta) { if (!paused) cooldown=std::max(0.0,cooldown-std::max(0.0,delta)); }
    void cast() { if (!paused && cooldown==0) { ++casts; cooldown=kCooldown; } }
    void use(std::size_t index) { if (index<item_counts.size() && !paused && item_counts[index]>0) { selected_item=index; --item_counts[index]; } }
};
}
