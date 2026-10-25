# 调试绘制

本文说明如何显示用于排查问题的区域和图元，以及谁负责清理这些内容。调试绘制只提供可视化，不改变碰撞或游戏规则。

`ELYSIA_DEBUG_DRAW` 收集用于诊断的图元，场景渲染时统一显示。它不创建游戏对象或物理碰撞体，也不替代正式美术渲染。全局启用与类别启用必须同时满足。

## 绘制游戏区域

下面函数可由场景更新调用，前提是场景已显式配置 `CameraSceneConfig`；没有相机时不能绘制这些世界图元，接入见[相机指南](../systems/camera.md)。示例假定本场景统一管理 Gameplay 类别，先清理该类别，再提交本帧全部图元；不要让多个模块各自清理同一类别。

```cpp
#include "engine/tools/debug_draw.h"

void show_interaction_area(const elysia::core::Rect& world_rect)
{
    using elysia::tools::DebugDrawCategory;
    elysia::tools::DebugDraw* draw = ELYSIA_DEBUG_DRAW;
    draw->set_enabled(true);
    // 保留其他系统已启用的类别，只添加本例所需的 Gameplay。
    draw->set_enabled_categories(
        draw->enabled_categories() | DebugDrawCategory::Gameplay);
    // 本例负责该类别的全部图元，每次先清理，再提交当前世界区域。
    draw->clear_categories(DebugDrawCategory::Gameplay);
    draw->draw_rect(DebugDrawCategory::Gameplay, world_rect,
                    elysia::core::Color{0, 255, 0, 255}, 2.0f);
}
```

通常只在开发设置变化时调用启用接口；这里为展示完整前提而放在同一函数中。使用矩形、圆、线、点分别调用 `draw_rect`、`draw_circle`、`draw_line`、`draw_point`。

## 坐标与类别

图元位置、矩形尺寸和圆半径使用世界坐标，经当前场景相机投影。线宽以及点的直径使用屏幕逻辑单位，不随相机缩放。它们不是窗口物理像素大小的保证。

`set_enabled_categories` 和 `clear_categories` 可组合多个类别；单次 `draw_*` 要指定一个有效类别，不能把多个类别的掩码直接当图元类别。未启用、无效类别、非有限坐标、空矩形、非正半径/线宽/点尺寸等无效请求会被忽略，不返回可供判错的布尔值。

## 命令保留与清理

命令不会仅因渲染过一次而自动清空。逐帧追加但不清理会积累旧图元，应按明确所有权使用 `clear_categories()` 或统一 `clear()`。关闭总开关会清空命令；关闭类别会清除该类别已有命令。场景切换流程也会清理调试命令。

物理类别由场景更新根据物理调试快照刷新；游戏自定义图元宜使用 Gameplay 等相应类别，不要依赖手写物理类别图元长期保留。当前场景在世界对象之后、普通 UI 之前绘制调试图元。

Dear ImGui 开发覆盖层可提供调试面板，`DebugDraw` 本身是独立服务，不要求游戏自行调用 ImGui 来绘制这些图元。

## 物理细节开关

调试总开关初始关闭。`DebugDrawCategory::Default` 保留基础物理轮廓、锚点等原有类别，排除新增的三种细节；`All` 才包含全部类别。物理示例默认采用 `Default`，可在 Inspector 或快捷键中单独开启：

| 类别 | 示例快捷键 | 展示 |
| --- | --- | --- |
| `PhysicsJointDetails` | F2 | 八类关节的轴、参考角、启用的限位、弹簧与目标；Filter 没有机械约束细节 |
| `PhysicsChainDetails` | F3 | 实际顶点、边段及开放链首尾 ghost；基础轮廓仍显示正面箭头 |
| `PhysicsTarget` | F4 | 运动学目标轮廓、朝向和当前位置到目标的连线 |

快捷键在关闭 ImGui 的物理示例中也可用；滚动状态栏末尾提供提示。关闭类别立即移除已有命令，场景退出和重新进入会清理旧图元。禁用、失活、销毁和未完成注册的对象不进入几何与速度快照；关节任一端不可见时整条隐藏，休眠对象仍显示。

关节锚点、局部轴、限位、Chain 顶点根据前后刚体姿态插值。Mouse 的世界目标保持世界坐标；Motor 的局部目标随第一端姿态重建。反作用力、接触和测量值表示最近物理状态，不进行显示插值。辅助轴长为 0.25 米对应的 EU、角弧半径为 0.2 米、箭头头长为 0.04 米；点直径和线宽继续使用逻辑屏幕单位。

运动学目标只保存待模拟命令和最近完成步的诊断数据：暂停或没有固定步时保留；下一固定步没有新目标时清除。步后回调的新目标标记为等待下一步。同一队列中后提交的目标覆盖前一个，显式设速、角速度、传送、禁用、移除与 Reset 清除展示。固定旋转保存请求角度，但轮廓使用实际生效角度。这些记录不驱动持续追踪；目标轮廓使用半透明青色，ghost 使用橙色虚线。

独立管理世界时，使用共享映射选择捕获类别；以下提交函数应在该世界的 `finalize_frame` 之后调用，并由调用方独占物理调试类别的清理。场景自身的物理世界已经执行这些步骤，不要再次提交。

```cpp
#include "engine/physics/physics_world.h"
#include "engine/physics/physics_debug_draw.h"

void enable_physics_details()
{
    using C = elysia::tools::DebugDrawCategory;
    auto* draw = ELYSIA_DEBUG_DRAW;
    draw->set_enabled(true);
    draw->set_enabled_categories(C::Default | C::PhysicsJointDetails |
        C::PhysicsChainDetails | C::PhysicsTarget);
}

void submit_owned_physics_debug(elysia::physics::PhysicsWorld& world)
{
    auto* draw = ELYSIA_DEBUG_DRAW;
    world.set_debug_capture(elysia::physics::physics_debug_capture_for(*draw));
    draw->clear_categories(elysia::physics::kPhysicsDebugCategories);
    elysia::physics::submit_physics_debug_snapshot(world.debug_snapshot(), *draw);
}
```

## 参考

- [图元与类别定义](../../../../engine/tools/debug_draw.h)
- [开发覆盖层](development-panels.md)、[物理功能](../systems/physics.md)
- [返回使用指南](../README.md)
