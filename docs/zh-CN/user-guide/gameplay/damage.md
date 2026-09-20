# 从攻击命中到接收伤害

本篇将[玩法碰撞](collision.md)、[物理](../systems/physics.md)和[对象生命周期](../scene/game-objects.md)串成一次攻击：开启 HitBox → 命中 HurtBox → 校验业务状态 → 扣除生命 → 表现与死亡 → 结束攻击。

引擎负责物理接触、阵营过滤、攻击实例去重和事件通知；生命值、攻击力、无敌、死亡与奖励由游戏实现。本例不新增引擎伤害接口，也不依赖角色动画或资源文件。

## 先区分三个标识

| 标识 | 本例用途 | 生命周期 |
| --- | --- | --- |
| ActorId | 找到攻击者与受击者的业务对象 | 角色存续期间稳定；处理旧事件前不能复用给另一角色 |
| AttackDefinitionId | 攻击种类，本例 1 表示造成 25 点伤害的普通攻击 | 可被多次攻击共享 |
| AttackInstanceId | 这一次挥击 | 同一次攻击的多个 HitBox 共享；下一次攻击使用新值 |

所有标识必须非零。ActorId 不是对象地址，AttackInstanceId 也不是动画帧号。多人同时攻击时由统一分配器确保当前碰撞运行时内实例号不冲突。

## 准备可受伤的对象

下面两个 C++ 块按顺序放在同一个游戏场景头文件中，构成一个可注册的示例场景；它不是包含应用入口的独立工程。对象同时持有一个 HurtBox 和一个初始禁用的 HitBox，两者使用独立过滤位，均为 Overlap，不推动对方。

```cpp
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/core/render/render_command.h"
#include "engine/tools/logger.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace damage_example {
namespace gc = elysia::gameplay::collision;
namespace ph = elysia::physics;

class Fighter final : public elysia::core::GameObject,
                      public ph::PhysicsParticipant {
public:
    Fighter(gc::ActorId actor, gc::TeamId faction, elysia::core::Vector2 position)
        : GameObject(elysia::core::DepthLayer::Character), id(actor), team(faction) {
        set_world_rect({position, {32.0f, 32.0f}});
        colliders_[0].shape = ph::AabbShape{{0, 0, 32, 32}};
        colliders_[0].filter = {.category = 2u, .mask = 1u};
        colliders_[0].response = ph::CollisionResponse::Overlap;
        colliders_[1].shape = ph::AabbShape{{24, 0, 40, 32}};
        colliders_[1].filter = {.category = 1u, .mask = 2u};
        colliders_[1].response = ph::CollisionResponse::Overlap;
        colliders_[1].enabled = false;
    }
    ph::BodyDefinition body_definition() const override {
        ph::BodyDefinition body;
        body.type = ph::BodyType::Dynamic;
        body.mass_policy = ph::MassPolicy::ExplicitMass;
        body.mass = 1.0f;
        body.gravity_scale = 0.0f;
        return body;
    }
    std::span<const ph::Collider> collider_definitions() const override {
        return colliders_;
    }
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const override {
        elysia::core::RenderCommand command;
        command.type = elysia::core::RenderCommandType::FillRect;
        command.command_rect = render_rect();
        command.color = team == gc::teams::Player
            ? elysia::core::Color{80, 160, 255} : elysia::core::Color{255, 100, 100};
        out.push_back(command);
    }
    void reset() noexcept override {
        GameObject::reset();
        hp = 40;
        invulnerable = false;
    }
    gc::ActorId id;
    gc::TeamId team;
    int hp = 40;
    bool invulnerable = false;
private:
    std::array<ph::Collider, 2> colliders_{};
};
} // namespace damage_example
```

物理对象加入场景后才能取得有效 ColliderId。不要把同一个 Collider 同时注册成 HurtBox 和 HitBox。两个静态刚体不适合作为这个重叠示例，因此这里显式使用 Dynamic；生产游戏应按自己的角色运动模型选择刚体类型。

## 开启攻击、接收伤害与清理

此场景进入后自动请求一次攻击。后续可由输入或控制命令调用 `queue_attack()`；示例不规定统一攻击按键。两角色的位置使攻击范围与目标 HurtBox 相交，第一次命中后目标生命从 40 降至 15，第二次有效攻击会死亡。

```cpp
namespace damage_example {
class DamageScene final : public elysia::gameplay::GameplayScene,
                          private gc::GameplayCollisionListener {
public:
    ~DamageScene() override { teardown(); }
    void on_enter(const elysia::scene::ScenePayload&) override {
        if (!attacker_) attacker_ = create_and_add_object<Fighter>(
            1, gc::teams::Player, elysia::core::Vector2{100, 100});
        if (!defender_) defender_ = create_and_add_object<Fighter>(
            2, gc::teams::Enemy, elysia::core::Vector2{140, 100});
        if (!attacker_ || !defender_ || !attacker_->physics_state() ||
            !defender_->physics_state() || !bind_fighter(*attacker_) ||
            !bind_fighter(*defender_)) {
            teardown();
            throw std::runtime_error("Damage example actor binding failed.");
        }
        listening_ = collision_runtime().add_listener(*this);
        if (!listening_) {
            teardown();
            throw std::runtime_error("Damage example listener binding failed.");
        }
        // 无跟随策略时明确设置视野，保证两个对象可见。
        ELYSIA_CAMERA->set_center(elysia::camera::CameraSlot::Main, {140, 116});
        resume();
        queued_ = true;
    }
    void on_exit() override { teardown(); }
    void reset() override {
        teardown();
        if (attacker_) attacker_->reset();
        if (defender_) defender_->reset();
    }
    void queue_attack() { if (listening_) queued_ = true; }
    void on_update(double delta) override {
        Scene::on_update(delta);
        // 此时物理回调与对象清理已完成；只使用队列中的值，不保存受击者指针。
        for (const auto& feedback : feedback_) {
            ELYSIA_LOG_INFO("combat", "Actor " << feedback.target
                << " took " << feedback.damage << ", hp=" << feedback.remaining_hp);
            // 可在这里向 UI 发消息、调用音频或特效服务。
        }
        feedback_.clear();
    }
protected:
    void on_game_fixed_update(std::uint64_t, double delta) override {
        if (active_attack_) {
            remaining_ -= delta;
            if (remaining_ <= 0.0) finish_attack();
            return; // 关闭后本步不重新开启，至少经过一次禁用状态的物理步。
        }
        if (!queued_) return;
        queued_ = false;
        if (!attacker_ || attacker_->is_destroyed() || attacker_->hp <= 0) return;
        if (next_attack_ == 0) throw std::overflow_error("Attack id exhausted.");
        active_attack_ = next_attack_++;
        const auto hit = attacker_->physics_collider(1);
        const bool bound = collision_runtime().bind_hit_box({
            .collider = {hit, attacker_->id, attacker_->team, gc::ColliderRole::HitBox},
            .instigator = attacker_->id,
            .attack_instance = active_attack_, .attack_definition = 1});
        if (!bound || !physics_world().set_collider_enabled(hit, true)) {
            finish_attack();
            ELYSIA_LOG_ERROR("combat", "Attack could not be enabled.");
            return;
        }
        remaining_ = 0.15; // 秒；下一次固定步开始递减。
    }
    void on_control_target_removing(elysia::core::SceneObject& object) override {
        if (&object == attacker_) {
            finish_attack();
            (void)collision_runtime().unbind_actor(attacker_->id);
            attacker_ = nullptr;
        }
        if (&object == defender_) {
            (void)collision_runtime().unbind_actor(defender_->id);
            defender_ = nullptr;
        }
    }
private:
    struct Feedback { gc::ActorId target; int damage; int remaining_hp; };
    bool bind_fighter(Fighter& actor) {
        gc::ActorCollisionRig rig;
        rig.owner = actor.id;
        rig.team = actor.team;
        rig.hurt_boxes = {actor.physics_collider(0)};
        return collision_runtime().bind_actor(rig);
    }
    Fighter* find_actor(gc::ActorId id) const {
        if (attacker_ && attacker_->id == id) return attacker_;
        if (defender_ && defender_->id == id) return defender_;
        return nullptr;
    }
    void on_hit_overlap(const gc::HitOverlapEvent& event) override {
        if (event.phase != ph::CollisionEventPhase::Begin ||
            !active_attack_ || event.hit_box.attack_instance != active_attack_ ||
            event.hit_box.attack_definition != 1) return;
        auto* source = find_actor(event.hit_box.instigator);
        auto* target = find_actor(event.hurt_box.owner);
        if (!source || !target || source == target || source->is_destroyed() ||
            target->is_destroyed() || source->hp <= 0 || target->hp <= 0 ||
            target->invulnerable) return;
        constexpr int damage = 25;
        target->hp = std::max(0, target->hp - damage);
        feedback_.push_back({target->id, damage, target->hp});
        if (target->hp == 0) target->destroy(); // 标记，不能 delete。
    }
    void finish_attack() {
        if (attacker_ && physics_world().contains_collider(attacker_->physics_collider(1)))
            (void)physics_world().set_collider_enabled(attacker_->physics_collider(1), false);
        if (active_attack_) collision_runtime().end_attack_instance(active_attack_);
        active_attack_ = 0;
        remaining_ = 0.0;
    }
    void teardown() {
        finish_attack();
        if (listening_) (void)collision_runtime().remove_listener(*this);
        listening_ = false;
        if (attacker_) (void)collision_runtime().unbind_actor(attacker_->id);
        if (defender_) (void)collision_runtime().unbind_actor(defender_->id);
        queued_ = false;
        feedback_.clear();
    }
    Fighter* attacker_ = nullptr;
    Fighter* defender_ = nullptr;
    bool listening_ = false, queued_ = false;
    gc::AttackInstanceId active_attack_ = 0, next_attack_ = 1;
    double remaining_ = 0.0;
    std::vector<Feedback> feedback_;
};
} // namespace damage_example
```

在游戏模块的 `register_scenes` 中选择一个未占用的游戏 SceneKey，调用 `register_game_scene<damage_example::DamageScene>(key)`，资源初始化完成后路由进入。示例没有控制器，因此无需仅为碰撞开始控制会话；接入玩家控制时按[控制器指南](control.md)创建会话和绑定。

示例使用派生场景自己的 `collision_runtime()`，确保退出阶段仍能清理本场景。其他游戏代码可通过 `elysia::gameplay::collision::GameplayCollisionService::instance()` 操作活动运行时；不要在退出时假设全局服务仍绑定旧场景。

## 为什么不能只收到事件就扣血

- **物理与玩法过滤分别生效**：category/mask 必须双向匹配，还要正确注册角色、HitBox/HurtBox 和敌对阵营。instigator 也必须已通过 bind_actor 注册；只绑定攻击 Collider 不足以形成完整命中条件。
- **一次命中不等于一定受伤**：无敌、死亡、格挡等由游戏判断。本例无敌时拒绝扣血，但该实例对该角色的命中已被引擎去重，不会在无敌结束后自动补发。
- **Begin 才触发攻击通知**：仅替换仍重叠的 HitBox 绑定或实例号，不保证生成新的 Begin。本例在两次攻击之间让物理实际经过一次 HitBox 禁用状态；不能在同一物理步前关闭又开启来保证重新命中。
- **多碰撞体去重**：同一次攻击的多个 HitBox 使用相同实例号，受击者的多个 HurtBox 使用同一 ActorId。同一实例可分别命中多个角色，但不会对同一角色重复通知。不同监听器仍会收到同一事件，因此扣血规则只由一个负责人执行。
- **高速攻击**：Overlap 不自动具备连续命中保障。需要扫掠查询时，游戏负责把查询结果交给同一伤害规则并维护去重；物理查询不会自动生成此处的 HitOverlapEvent。

## 攻击取消、死亡与场景退出

攻击结束或被打断时同时禁用物理 HitBox、调用 `end_attack_instance`。后者只清除玩法绑定和去重，不销毁或禁用物理形状。一次攻击窗口过短、未覆盖任何实际物理步时，可能完全没有碰撞结果。

本例暂停时固定步停止，攻击窗口随之冻结；若设计要求暂停立即取消攻击，在暂停操作旁调用游戏自己的取消逻辑，不覆盖 GameplayScene 的 final 暂停回调。

扣血回调只改业务状态并记录值类型表现请求。`destroy()` 标记死亡后，同帧后续事件可能仍到达，因此再次处理时检查存活状态。创建掉落物、音效与特效放在回调返回后的安全阶段；不要在遍历中新增场景对象，也不要捕获死亡对象指针留给延迟回调。

在 `on_control_target_removing` 中解除玩法关联并清空借用指针；不要覆盖 GameplayScene 的 final `on_scene_object_removing`。退出和析构解除监听，监听器本身必须活过当前事件分发。本例退出保留存活对象，重置恢复生命；Recreate 会创建全新实例。生产项目若延迟处理事件，应使用带代次的角色标识或其他可验证句柄，避免旧事件命中新角色。

## 验证自己的接入

1. 敌对双方重叠攻击：一次攻击只扣一次；第二次攻击使用新实例并经历有效启停后再次扣血。
2. 同队、物理 mask 不匹配或无敌：不扣血；确认无敌结束不自动补发同一次攻击。
3. 同一目标多个 HurtBox：仍只扣一次；多个目标分别命中。
4. 攻击取消、目标死亡、退出或重进场景：没有残留攻击绑定、重复监听和悬空指针。
5. 低帧率、零个或多个固定步：不按渲染帧重复扣血，表现队列不依赖已被清理的对象。

这些是接入后的运行检查建议，本页示例的语法检查不替代项目实际运行验证。

- [玩法碰撞](collision.md)、[GameplayScene](scene.md)、[物理](../systems/physics.md)
- [动画与特效](../systems/animation-and-effects.md)、[音频](../systems/audio.md)、[返回使用指南](../README.md)
