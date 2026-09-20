# 相机与坐标

通过 `ELYSIA_CAMERA` 配置相机，通过 `Scene::camera()` 或 `camera(slot)` 读取只读视图。标准 Scene 已负责每帧更新，相机服务不需要游戏重复驱动。

## 配置跟随

以下辅助函数放在游戏代码中，在场景进入时调用。逻辑画布使用项目 ApplicationDescriptor 中的尺寸，此处 1280×720 只是示例。

```cpp
#include "engine/camera/camera_manager.h"
#include "engine/camera/follow_strategy.h"
#include <memory>

void configure_camera() {
    using namespace elysia::camera;
    auto* cameras = ELYSIA_CAMERA;
    cameras->set_viewport_size(CameraSlot::Main, {1280.0f, 720.0f});
    cameras->set_zoom(CameraSlot::Main, 1.0f);
    cameras->set_world_bounds(CameraSlot::Main,
        elysia::core::Rect{0, 0, 3000, 2000});
    cameras->set_follow_strategy(CameraSlot::Main,
        std::make_unique<SmoothFollowStrategy>(300.0));
}
```

在场景类中重写 `resolve_camera_focus_rect()` 返回目标的 `render_rect()`；没有有效目标时返回 `std::nullopt`。场景持有的目标指针必须在对象移除时清空。Main 每帧接收此回调结果，不要同时把手工 `set_focus` 作为 Main 的持久焦点来源。

## 常用操作

| 任务 | 接口与行为 |
| --- | --- |
| 固定镜头 | `set_center(slot, position)`，配合无跟随策略 `set_follow_strategy(slot, nullptr)` |
| 直接跟随 | `HardFollowStrategy` |
| 匀速追踪 | `SmoothFollowStrategy(speed)`，速度为世界单位/秒 |
| 死区跟随 | `DeadZoneFollowStrategy(rect)`，矩形是视口局部逻辑坐标 |
| 限制地图范围 | `set_world_bounds(slot, rect)`；传 nullopt 移除 |
| 即时缩放 | `set_zoom(slot, value)`，取消该槽位平滑变焦 |
| 平滑缩放 | `request_zoom_to(slot, target, seconds)` |
| 震屏 | `request_shake(slot, CameraShakeParams{...})`，参数为 amplitude、duration_seconds、frequency_hz |
| 对焦与清除效果 | `request_snap_to_focus(slot)`、`request_clear_effects(slot)` |

setter 直接设置配置，请求类接口在下一次相机更新处理。新的震屏替换旧震屏，新的变焦从当前倍率接续；清除效果保留当前倍率与逻辑中心。zoom 范围为 0.1–10，非有限倍率回退到 1；时长不大于零的变焦立即完成。游戏仍应传入有限且有意义的坐标与时间。

## 多目标与相机槽位

包含 `engine/camera/multi_target_follow_strategy.h` 后，可将策略设为 `std::make_unique<elysia::camera::MultiTargetFollowStrategy>()`。重写场景 `resolve_camera_focus()`，把目标矩形组成数组，返回 `elysia::camera::make_camera_focus(rects, primary_index)`。该函数复制几何信息，不持有游戏对象；空列表或全部非法时返回 nullopt。

`MultiTargetFollowConfig` 可设置 safe_ratio、inner_ratio、min_zoom/max_zoom、movement_half_life、zoom_out_half_life、zoom_in_half_life、settle_seconds 和 dead_zone_enabled。安全区域要求 `0 < inner_ratio < safe_ratio <= 1`；配置通过策略构造函数或 `set_config` 传入，后者重置策略计时状态。半衰期越小响应越快，settle_seconds 控制收拢后的放大等待。

多目标策略同时移动和缩放，放大有等待，缩小较快；无法在最小倍率容纳全部目标时优先主目标。手动平滑变焦期间自动缩放让位，之后恢复。平滑追踪、地图边界和震屏都可能使目标暂时离开屏幕，不保证每一帧包含所有目标。

固定槽位为 Main、Cinematic、Auxiliary1、Auxiliary2。在派生 Scene 中调用受保护的 `set_render_camera_slot(slot)` 选择世界渲染相机。其他槽位的 focus 由业务显式设置；多个槽位不等于已经提供分屏布局。

## 坐标转换

`world_to_screen(point/rect)` 输出视口局部逻辑渲染坐标；`screen_to_world` 进行反向转换。逻辑可见世界尺寸等于 `viewport_size / zoom`，矩形转换同时改变位置与尺寸。

引擎 InputSystem 会先把窗口坐标转为逻辑坐标。用接收到的逻辑指针位置调用当前渲染相机的 `screen_to_world`，才得到世界瞄准位置；相机投影结果不能直接当作实际窗口像素或容器局部坐标。UI 绘制不经过世界相机。

读取对象屏幕显示位置时，对其 `render_rect()` 做投影；游戏判定仍使用 `world_rect()`。不要在提交世界绘制命令前自行投影，否则会重复转换。

## 场景切换与常见误用

切换到不同场景或 Reset 当前场景时，Main 会被重置，保留视口，清除焦点、边界、策略、效果并恢复倍率；进入回调重新配置。Reuse 当前实例仍执行退出/进入，但不重置 Main。其他槽位跨场景保留，业务结束使用后调用 `reset(slot)` 或重新配置。

忘记调用基类 Scene 更新会停止相机调度。手动控制与跟随策略同时启用可能相互影响；选择好该阶段的控制方式。所有请求按主线程使用，不跨线程操作相机队列。

## 参考

- [场景](../scene/scene.md)、[输入](input.md)、[相机接口](../../../../engine/camera/camera_manager.h)
- [可选架构说明](../../architecture/subsystems/camera.md)、[返回使用指南](../README.md)
