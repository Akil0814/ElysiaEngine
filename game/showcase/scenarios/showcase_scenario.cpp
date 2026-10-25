#include "game/showcase/shared/collider_render.h"
#include "game/showcase/scenarios/showcase_scenario_internal.h"
#include "game/navigation/showcase_scene_keys.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace example::showcase::scenarios
{
using namespace example::showcase::gameplay;
namespace
{
using C = ScenarioCategory;
constexpr ScenarioDescriptor catalog[] = {
    {"edges", C::Physics, example::scene_keys::Box2DLab, 360},
    {"target_pose", C::Physics, example::scene_keys::Box2DLab, 360},
    {"joint_weld", C::Physics, example::scene_keys::Box2DLab, 360},
    {"joint_prismatic", C::Physics, example::scene_keys::Box2DLab, 360},
    {"joint_wheel", C::Physics, example::scene_keys::Box2DLab, 360},
    {"joint_mouse", C::Physics, example::scene_keys::Box2DLab, 360},
    {"joint_motor", C::Physics, example::scene_keys::Box2DLab, 360},
    {"joint_filter", C::Physics, example::scene_keys::Box2DLab, 360},
    {"geometry", C::Physics, example::scene_keys::Box2DLab, 360},
    {"shape_queries", C::Physics, example::scene_keys::Box2DLab, 10},
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
    {"combat_collider", C::Gameplay, example::scene_keys::GameplayVerification, 480},
    {"combat_platform", C::Gameplay, example::scene_keys::GameplayVerification, 480},
    {"combat_topdown", C::Gameplay, example::scene_keys::GameplayVerification, 480},
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
std::span<const ScenarioDescriptor> showcase_scenarios() { return catalog; }
const ScenarioDescriptor* find_showcase_scenario(std::string_view id)
{
    for (const auto& c : catalog) if (c.id == id) return &c;
    return nullptr;
}
std::string scenario_status_key(ScenarioStatus s)
{
    const char* names[] = {"ready", "running", "passed", "failed"};
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
    using namespace elysia::core;
    for (const auto& shape : colliders)
    {
        if (!shape.enabled || shape.response == CollisionResponse::Ignore) continue;
        Color color = shape.response == CollisionResponse::Overlap ? Color{168,105,210,150}
                      : definition.type == BodyType::Static ? Color{105,120,140}
                      : state->awake ? Color{240,180,75} : Color{70,145,180};
        example::showcase::submit_collider_shape(shape.shape,pose,color,out);
    }
}
ShowcaseScenario::Impl::Impl(const ScenarioDescriptor& d, int t)
    : descriptor(d), tier(t), world(config()), runtime(world), combat(world,runtime)
{
    if (t < 0 || t > 2) throw std::invalid_argument("Invalid pressure tier");
    world.add_listener(*this);
    build();
    if (finish_at > d.max_steps) throw std::logic_error("Scenario exceeds step budget");
    expected_objects = world.registered_object_count();
}
ShowcaseScenario::Impl::~Impl()
{
    for (const auto& object : objects)
        if (auto* actor = dynamic_cast<BlockCombatActor*>(object.get())) combat.unregister_actor(*actor);
    // Owners must outlive their registration; reset before member destruction.
    world.reset();
}
ScenarioBody& ShowcaseScenario::Impl::body(Vector2 p, Vector2 size, BodyType type, bool gravity, bool circle)
{
    BodyDefinition d;
    d.type = type;
    d.gravity_scale = gravity ? 1.f : 0.f;
    Collider c;
    c.filter.category = collision_layers::Body;
    c.shape = circle ? ColliderShape{CircleShape{size * 0.5f,size.x * 0.5f}}
                     : ColliderShape{BoxShape::from_rect({0,0,size.x,size.y})};
    return add<ScenarioBody>(p,d,std::vector<Collider>{c});
}
DemoTileMap& ShowcaseScenario::Impl::tile_map(int cols, int rows, Vector2 size, Vector2 origin, TileOutOfBoundsPolicy policy)
{
    tiles = &add<DemoTileMap>(origin,size,cols,rows,policy);
    return *tiles;
}
void ShowcaseScenario::Impl::check(std::string id, double actual, double expected, double tolerance)
{
    const bool ok = std::isfinite(actual) && std::abs(actual-expected) <= tolerance;
    result.checks.push_back({std::move(id),ok,actual,expected,tolerance});
    if (!ok)
    {
        result.status = ScenarioStatus::Failed;
        if (result.failure.empty()) result.failure = result.checks.back().id;
    }
}
void ShowcaseScenario::Impl::complete()
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
void ShowcaseScenario::Impl::step()
{
    if (result.status != ScenarioStatus::Running) return;
    const unsigned n = ++result.steps;
    if (before) before(n);
    combat.update(1.0/60);
    world.step(1.0/60);
    combat.flush_deaths();
    result.stats = world.last_step_stats();
    if (descriptor.category == ScenarioCategory::Stress && n > 120)
        timings.push_back(result.stats.step_milliseconds);
    if (after) after(n);
    if (!manual_joint_control && (n >= finish_at || result.status == ScenarioStatus::Failed)) complete();
    else if (!manual_joint_control && n >= descriptor.max_steps)
    {
        truth("timeout",false);
        complete();
    }
    // Keep event history bounded even for the dense-contact workload.
    if (descriptor.category == ScenarioCategory::Stress) events.clear();
}
void ShowcaseScenario::Impl::build()
{
    const auto id = descriptor.id;
    if(id=="edges")build_edges();
    else if(id=="target_pose")build_target_pose();
    else if(id.starts_with("joint_"))build_joint_extensions();
    else if (id == "geometry") build_geometry();
    else if (id == "shape_queries") build_shape_queries();
    else if (id == "motion") build_motion();
    else if (id == "materials") build_materials();
    else if (id == "ccd") build_ccd();
    else if (id == "filters") build_filters();
    else if (id == "queries") build_queries();
    else if (id == "one_way") build_one_way();
    else if (id == "tiles") build_tiles();
    else if (id == "joints") build_joints();
    else if (id == "lifecycle") build_lifecycle();
    else if (id == "timing") build_timing();
    else if (descriptor.category == ScenarioCategory::Gameplay) build_combat();
    else if (descriptor.category == ScenarioCategory::Stress) build_stress();
    else throw std::invalid_argument("Unimplemented scenario");
}
ShowcaseScenario::ShowcaseScenario(std::string_view id, int tier)
{
    auto* d = find_showcase_scenario(id);
    if (!d) throw std::invalid_argument("Unknown physics scenario");
    _impl = std::make_unique<Impl>(*d,tier);
}
ShowcaseScenario::~ShowcaseScenario() = default;
void ShowcaseScenario::start()
{
    if (_impl->result.status == ScenarioStatus::Ready) _impl->result.status = ScenarioStatus::Running;
}
void ShowcaseScenario::advance(double dt)
{
    auto& p = *_impl;
    if (p.paused || !std::isfinite(dt) || dt <= 0 || p.result.status != ScenarioStatus::Running) return;
    p.fixed_step.advance(std::min(dt, 0.25),
        [&p](std::uint64_t, double) { p.step(); });
    p.world.finalize_frame(p.fixed_step.stats().interpolation_alpha);
    p.result.fixed_step_stats = p.fixed_step.stats();
}
void ShowcaseScenario::single_step()
{
    if (!_impl->paused) return;
    start();
    _impl->step();
    _impl->world.finalize_frame(0.0);
}
void ShowcaseScenario::set_paused(bool p) { _impl->paused=p; }
bool ShowcaseScenario::paused() const { return _impl->paused; }
void ShowcaseScenario::set_debug_capture(PhysicsDebugCapture capture)
{
    auto& p = *_impl;
    const auto previous=p.world.debug_capture();
    p.world.set_debug_capture(capture);
    const auto effective=p.world.debug_capture();
    if (effective != previous && !p.timings.empty()) p.result.mixed_debug_samples=true;
    p.result.debug_geometry=effective!=PhysicsDebugCapture::None;
}
void ShowcaseScenario::finish_timeout() { _impl->truth("timeout",false); _impl->complete(); }
const ScenarioDescriptor& ShowcaseScenario::descriptor() const { return _impl->descriptor; }
const ScenarioResult& ShowcaseScenario::result() const { return _impl->result; }
const PhysicsWorld& ShowcaseScenario::world() const { return _impl->world; }
elysia::core::Rect ShowcaseScenario::bounds() const { return _impl->view; }
void ShowcaseScenario::submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const
{
    for (const auto& o : _impl->objects)
        if (!o->is_destroyed() && o->is_visible()) o->submit_render_commands(out);
    out.insert(out.end(),_impl->query_commands.begin(),_impl->query_commands.end());
    for (const auto h : _impl->joints)
        if (auto state = _impl->world.joint_state(h))
        {
            if(std::holds_alternative<FilterJointState>(state->details))
                for(int i=0;i<10;i+=2)out.push_back(elysia::core::make_world_draw_line_command(
                    state->anchor_first+(state->anchor_second-state->anchor_first)*(i/10.f),
                    state->anchor_first+(state->anchor_second-state->anchor_first)*((i+1)/10.f),{150,160,180},1));
            else out.push_back(elysia::core::make_world_draw_line_command(state->anchor_first,state->anchor_second,{110,210,220},2));
        }
}
}

namespace example::showcase::scenarios
{
void ShowcaseScenario::control_joints()
{
    if (_impl->descriptor.id!="joints"&&!_impl->descriptor.id.starts_with("joint_")) return;
    if (_impl->result.status==ScenarioStatus::Passed || _impl->result.status==ScenarioStatus::Failed)
        _impl=std::make_unique<Impl>(_impl->descriptor,_impl->tier);
    auto& p=*_impl;p.manual_joint_control=true;
    p.paused=false;p.result.status=ScenarioStatus::Running;
    if(p.descriptor.id.starts_with("joint_")){p.toggle_extension_joint();return;}
    for(auto h:p.joints) if(auto state=p.world.joint_state(h))
    {
        if(auto* distance=std::get_if<elysia::physics::DistanceJointState>(&state->details))
        {
            auto settings=distance->settings;settings.length=settings.length>85?70:100;
            settings.enable_spring=true;p.world.update_distance_joint(h,settings);
        }
        else if(auto* hinge=std::get_if<elysia::physics::RevoluteJointState>(&state->details))
        {
            auto settings=hinge->settings;
            settings.motor_speed=-settings.motor_speed;p.world.update_revolute_joint(h,settings);
        }
    }
}
}
