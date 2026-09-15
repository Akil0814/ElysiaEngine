#include "physics_scenario_internal.h"
#include "../../scene/example_scene_keys.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace example::demo::physics
{
namespace
{
using C = ScenarioCategory;
constexpr ScenarioDescriptor catalog[] = {
    {"motion", C::Physics, example::scene_keys::Box2DLab, 180},
    {"materials", C::Physics, example::scene_keys::Box2DLab, 900},
    {"ccd", C::Physics, example::scene_keys::Box2DLab, 10},
    {"filters", C::Physics, example::scene_keys::Box2DLab, 20},
    {"queries", C::Physics, example::scene_keys::Box2DLab, 10},
    {"one_way", C::Physics, example::scene_keys::Box2DLab, 120},
    {"tiles", C::Physics, example::scene_keys::Box2DLab, 720},
    {"joints", C::Physics, example::scene_keys::Box2DLab, 480},
    {"lifecycle", C::Physics, example::scene_keys::Box2DLab, 20},
    {"timing", C::Physics, example::scene_keys::Box2DLab, 180},
    {"combat_collider", C::Combat, example::scene_keys::ColliderCombatDemo, 480},
    {"combat_platform", C::Combat, example::scene_keys::PlatformTileCombatDemo, 480},
    {"combat_topdown", C::Combat, example::scene_keys::TopDownTileCombatDemo, 480},
    {"stress_bodies", C::Stress, example::scene_keys::Box2DLab, 720},
    {"stress_contacts", C::Stress, example::scene_keys::Box2DLab, 720},
    {"stress_tiles", C::Stress, example::scene_keys::Box2DLab, 720},
};
std::map<std::pair<std::string,int>, ScenarioResult> recent;
PhysicsWorldConfig config()
{
    PhysicsWorldConfig c;
    c.gravity = {0, 980};
    return c;
}
}
std::string ScenarioDescriptor::title_key() const { return "physics_tests.cases." + std::string(id) + ".title"; }
std::string ScenarioDescriptor::purpose_key() const { return "physics_tests.cases." + std::string(id) + ".purpose"; }
std::string ScenarioDescriptor::expected_key() const { return "physics_tests.cases." + std::string(id) + ".expected"; }
std::span<const ScenarioDescriptor> physics_scenarios() { return catalog; }
const ScenarioDescriptor* find_physics_scenario(std::string_view id)
{
    for (const auto& c : catalog) if (c.id == id) return &c;
    return nullptr;
}
std::string_view default_physics_scenario(elysia::scene::SceneKey scene)
{
    for (const auto& c : catalog) if (c.scene == scene) return c.id;
    return "motion";
}
std::string scenario_status_key(ScenarioStatus s)
{
    const char* names[] = {"ready", "running", "passed", "failed", "free_play"};
    return "physics_tests." + std::string(names[static_cast<int>(s)]);
}
void remember_scenario_result(std::string_view id, int tier, const ScenarioResult& r)
{
    recent[{std::string(id), tier}] = r;
}
const ScenarioResult* recent_scenario_result(std::string_view id, int tier)
{
    auto it = recent.find({std::string(id),tier});
    return it == recent.end() ? nullptr : &it->second;
}
ScenarioBody::ScenarioBody(Vector2 p, BodyDefinition d, std::vector<Collider> c)
    : GameObject(elysia::core::DepthLayer::Item), definition(d), colliders(std::move(c))
{ set_position(p); }
void ScenarioBody::submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const
{
    auto state = physics_state();
    if (!state || !state->enabled) return;
    auto pose = physics_world()->render_pose(physics_handle()).value();
    const auto rotate = [&](Vector2 v) {
        const float c = std::cos(pose.angle), s = std::sin(pose.angle);
        return pose.position + Vector2{c*v.x-s*v.y, s*v.x+c*v.y};
    };
    using namespace elysia::core;
    for (const auto& shape : colliders)
    {
        if (!shape.enabled || shape.response == CollisionResponse::Ignore) continue;
        Color color = shape.response == CollisionResponse::Overlap ? Color{168,105,210,150}
                      : definition.type == BodyType::Static ? Color{105,120,140}
                      : state->awake ? Color{240,180,75} : Color{70,145,180};
        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T,CircleShape>)
                out.push_back(make_world_fill_circle_command(rotate(s.local_center), s.radius, color));
            else
            {
                const auto r = s.local_rect;
                auto a = rotate({r.left(),r.top()}), b = rotate({r.right(),r.top()}),
                     c = rotate({r.right(),r.bottom()}), d = rotate({r.left(),r.bottom()});
                out.push_back(make_world_fill_triangle_command(a,b,c,color));
                out.push_back(make_world_fill_triangle_command(a,c,d,color));
            }
        }, shape.shape);
    }
}
PhysicsScenario::Impl::Impl(const ScenarioDescriptor& d, int t)
    : descriptor(d), tier(t), world(config()), runtime(world), combat(world,runtime)
{
    if (t < 0 || t > 2) throw std::invalid_argument("Invalid pressure tier");
    world.add_listener(*this);
    build();
    if (finish_at > d.max_steps) throw std::logic_error("Scenario exceeds step budget");
    expected_objects = world.registered_object_count();
}
PhysicsScenario::Impl::~Impl()
{
    for (const auto& object : objects)
        if (auto* actor = dynamic_cast<BlockCombatActor*>(object.get())) combat.unregister_actor(*actor);
    // Owners must outlive their registration; reset before member destruction.
    world.reset();
}
ScenarioBody& PhysicsScenario::Impl::body(Vector2 p, Vector2 size, BodyType type, bool gravity, bool circle)
{
    BodyDefinition d;
    d.type = type;
    d.gravity_scale = gravity ? 1.f : 0.f;
    Collider c;
    c.filter.category = collision_layers::Body;
    c.shape = circle ? ColliderShape{CircleShape{size * 0.5f,size.x * 0.5f}}
                     : ColliderShape{AabbShape{{0,0,size.x,size.y}}};
    return add<ScenarioBody>(p,d,std::vector<Collider>{c});
}
DemoTileMap& PhysicsScenario::Impl::tile_map(int cols, int rows, Vector2 size, Vector2 origin, TileOutOfBoundsPolicy policy)
{
    tiles = &add<DemoTileMap>(origin,size,cols,rows,policy);
    return *tiles;
}
void PhysicsScenario::Impl::check(std::string id, double actual, double expected, double tolerance)
{
    const bool ok = std::isfinite(actual) && std::abs(actual-expected) <= tolerance;
    result.checks.push_back({std::move(id),ok,actual,expected,tolerance});
    if (!ok)
    {
        result.status = ScenarioStatus::Failed;
        if (result.failure.empty()) result.failure = result.checks.back().id;
    }
}
void PhysicsScenario::Impl::complete()
{
    if (result.checks.empty()) truth("checks.nonempty",false);
    if (result.status != ScenarioStatus::Failed) result.status = ScenarioStatus::Passed;
    if (!timings.empty())
    {
        auto sorted = timings;
        std::ranges::sort(sorted);
        result.samples = sorted.size();
        result.median_ms = sorted[sorted.size()/2];
        result.p95_ms = sorted[static_cast<std::size_t>(std::ceil(sorted.size()*.95))-1];
        result.max_ms = sorted.back();
    }
}
void PhysicsScenario::Impl::step()
{
    if (result.status != ScenarioStatus::Running) return;
    const unsigned n = ++result.steps;
    if (before) before(n);
    combat.update(1.0/60);
    world.advance(1.0/60);
    combat.flush_deaths();
    result.stats = world.last_step_stats();
    if (descriptor.category == ScenarioCategory::Stress && n > 120)
        timings.push_back(result.stats.step_milliseconds);
    if (after) after(n);
    if (n >= finish_at || result.status == ScenarioStatus::Failed) complete();
    else if (n >= descriptor.max_steps)
    {
        truth("timeout",false);
        complete();
    }
    // Keep event history bounded even for the dense-contact workload.
    if (descriptor.category == ScenarioCategory::Stress) events.clear();
}
void PhysicsScenario::Impl::build()
{
    const auto id = descriptor.id;
    if (id == "motion") build_motion();
    else if (id == "materials") build_materials();
    else if (id == "ccd") build_ccd();
    else if (id == "filters") build_filters();
    else if (id == "queries") build_queries();
    else if (id == "one_way") build_one_way();
    else if (id == "tiles") build_tiles();
    else if (id == "joints") build_joints();
    else if (id == "lifecycle") build_lifecycle();
    else if (id == "timing") build_timing();
    else if (descriptor.category == ScenarioCategory::Combat) build_combat();
    else if (descriptor.category == ScenarioCategory::Stress) build_stress();
    else throw std::invalid_argument("Unimplemented scenario");
}
PhysicsScenario::PhysicsScenario(std::string_view id, int tier)
{
    auto* d = find_physics_scenario(id);
    if (!d) throw std::invalid_argument("Unknown physics scenario");
    _impl = std::make_unique<Impl>(*d,tier);
}
PhysicsScenario::~PhysicsScenario() = default;
void PhysicsScenario::start()
{
    if (_impl->result.status == ScenarioStatus::Ready) _impl->result.status = ScenarioStatus::Running;
}
void PhysicsScenario::advance(double dt)
{
    auto& p = *_impl;
    if (p.paused || !std::isfinite(dt) || dt <= 0 || p.result.status != ScenarioStatus::Running) return;
    // Bound work per display frame without dropping script ticks.
    p.accumulator += std::min(dt,0.25);
    for (int i=0; i<8 && p.accumulator + 1e-10 >= 1.0/60; ++i)
    {
        p.accumulator = std::max(0.0,p.accumulator - 1.0/60);
        p.step();
        if (p.result.status != ScenarioStatus::Running) { p.accumulator=0; break; }
    }
}
void PhysicsScenario::single_step() { if (_impl->paused) { start(); _impl->step(); } }
void PhysicsScenario::set_paused(bool p) { _impl->paused=p; }
bool PhysicsScenario::paused() const { return _impl->paused; }
void PhysicsScenario::set_debug_geometry(bool enabled)
{
    auto& p = *_impl;
    if (enabled != p.result.debug_geometry && !p.timings.empty()) p.result.mixed_debug_samples=true;
    p.result.debug_geometry=enabled;
    p.world.set_debug_capture(enabled ? PhysicsDebugCapture::All : PhysicsDebugCapture::None);
}
void PhysicsScenario::finish_timeout() { _impl->truth("timeout",false); _impl->complete(); }
const ScenarioDescriptor& PhysicsScenario::descriptor() const { return _impl->descriptor; }
const ScenarioResult& PhysicsScenario::result() const { return _impl->result; }
const PhysicsWorld& PhysicsScenario::world() const { return _impl->world; }
elysia::core::Rect PhysicsScenario::bounds() const { return _impl->view; }
void PhysicsScenario::submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const
{
    for (const auto& o : _impl->objects)
        if (!o->is_destroyed() && o->is_visible()) o->submit_render_commands(out);
    for (const auto h : _impl->joints)
        if (auto state = _impl->world.joint_state(h))
            out.push_back(elysia::core::make_world_draw_line_command(state->anchor_first,state->anchor_second,{110,210,220},2));
}
}
