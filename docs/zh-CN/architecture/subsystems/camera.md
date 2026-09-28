# Camera 工作流

## 模块结构

相机模块由三个层次组成：

```text
CameraManager（全局唯一拥有者和写入口）
  ├─ Main       Camera + CameraController
  ├─ Cinematic  Camera + CameraController
  ├─ Auxiliary1 Camera + CameraController
  └─ Auxiliary2 Camera + CameraController

SceneCameraRuntime（可选的场景能力）
  └─ owned slots、最终呈现槽、混合状态和场景句柄所有权
```

- `Camera` 保存最终渲染中心、屏幕视口大小和缩放倍率，并负责世界坐标与屏幕坐标之间的转换。
- `CameraController` 保存逻辑中心、焦点、世界边界、跟随策略、震屏和单槽姿态运动。
- `CameraManager` 固定拥有四组相机，不提供动态创建或销毁接口。
- `SceneCameraRuntime` 保存最终呈现槽和可选的槽间混合状态，并限制场景只能操作声明拥有的槽和句柄。
- `Scene` 只协调焦点解析、生命周期和完成事件派发，不保存相机播放状态。

四个槽位定义如下：

| 槽位 | 用途 |
| --- | --- |
| `Main` | Scene 默认使用的世界相机。 |
| `Cinematic` | 过场和演出视角。 |
| `Auxiliary1` | 无预设用途的扩展相机。 |
| `Auxiliary2` | 无预设用途的扩展相机。 |

## 访问规则

`CameraManager` 是底层相机服务。相机子系统可以直接使用它；场景代码应通过 `camera_runtime()` 操作本场景声明拥有的槽。外部只能取得 `const Camera&`，用于渲染、查询可见区域和坐标投影：

```cpp
const auto& camera = elysia::camera::CameraManager::instance()->camera(
    elysia::camera::CameraSlot::Main
);
```

相机状态必须通过 Manager 修改：

```cpp
auto* cameras = elysia::camera::CameraManager::instance();

cameras->set_viewport_size(CameraSlot::Main, { 1280.0f, 720.0f });
cameras->set_zoom(CameraSlot::Main, 2.0f);
cameras->set_world_bounds(CameraSlot::Main, world_bounds);
cameras->set_follow_strategy(
    CameraSlot::Main,
    std::make_unique<SmoothFollowStrategy>(300.0)
);
```

配置接口立即生效，包括中心、视口、缩放、焦点、边界和跟随策略。直接设置中心或缩放会取消该槽正在播放的姿态运动，并同步 Controller 与最终 Camera，避免两个控制源同时写入。

## 视口、缩放与坐标

`viewport_size` 使用屏幕像素，`zoom` 默认为 `1.0`，合法范围为 `[0.1, 10.0]`。普通越界值会被钳制，非有限值回退为 `1.0`。相机在世界中实际可见的尺寸为：

```text
world_viewport_size = viewport_size / zoom
```

`world_to_screen` 和 `screen_to_world` 同时支持点和矩形。矩形转换会同时变换位置与尺寸；屏幕坐标是 viewport-local 坐标，不含窗口位置或 UI 布局偏移。UI 命令不经过这些转换。

## 请求与更新时序

震屏、立即对焦和清除效果是 FIFO 瞬时请求；中心与缩放动画使用统一的姿态运动：

```cpp
cameras->request_shake(CameraSlot::Main, shake_params);
cameras->request_snap_to_focus(CameraSlot::Cinematic);
cameras->request_clear_effects(CameraSlot::Main);

const auto motion = cameras->move_camera_to(
    CameraSlot::Main, {.center = destination, .zoom = 2.0f}, 0.5);
```

请求按提交顺序进入单线程 FIFO 队列。`CameraManager::update(slots, delta)` 的执行顺序为：

1. 按 FIFO 顺序处理所有待执行请求。
2. 更新调用方指定的 `CameraSlotSet`。
3. Controller 推进活动姿态运动；播放期间暂时取得中心和缩放控制权。
4. 没有姿态运动时，Controller 应用跟随策略返回的中心与可选 zoom。
5. 按当前倍率限制世界边界，再叠加震屏偏移。
6. 最终中心写回 Camera，并返回本帧自然完成的 motion 值事件。

同一槽保存一个姿态运动和一个震屏，两者可以并行。路径节点使用从上一姿态到目标姿态的正时长和统一缓动；新运动从当前逻辑姿态连续替换旧运动。清除效果停止震屏但不取消姿态运动。直接设置姿态、吸附焦点或显式取消会结束运动，且不会产生自然完成事件。

DeadZone 使用 viewport-local 屏幕像素定义。焦点会先按当前 zoom 投影后再与死区比较，因此改变倍率不会改变死区在屏幕上的视觉大小。

## Scene 集成

Scene 不拥有 Camera 或 CameraController。相机是可选运行时能力，基础场景行为为：

- Scene 用可选的 `CameraSceneConfig` 声明相机运行时；默认 `Scene` 和 `GameplayScene` 都没有相机。所有 owned slots 都会推进；`ResolveEachFrame` 额外调用 `resolve_camera_focus(slot)`。
- 有相机的场景使用最终呈现相机投影世界命令。无相机场景可以正常渲染 UI，但提交世界命令或可见的世界 DebugDraw 属于配置错误。
- UI 命令仍直接使用屏幕坐标执行，不经过世界相机。
- `Scene::camera()` 返回最终呈现相机；活动混合期间返回 Scene 的组合相机，其余时间返回已提交槽。

```cpp
std::optional<elysia::camera::CameraFocus> MyScene::resolve_camera_focus(
    elysia::camera::CameraSlot slot) const
{
    if (slot != elysia::camera::CameraSlot::Main || !_player)
        return std::nullopt;
    const auto rect = _player->render_rect();
    return elysia::camera::CameraFocus{rect, rect};
}
```

场景若需要玩家与演出相机并行更新，可以配置：

```cpp
MyScene::MyScene() : Scene(elysia::scene::SceneRuntimeFeatures{
    .camera = elysia::scene::CameraSceneConfig{
        .initial_slot = elysia::camera::CameraSlot::Main,
        .owned_slots = elysia::camera::CameraSlot::Main
                     | elysia::camera::CameraSlot::Cinematic,
        .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}}) {}
```

`camera_runtime().cut_to()` 立即提交槽；`camera_runtime().blend_to()` 捕获当前实际呈现构图作为固定源，并逐帧插值到实时目标槽。中心和缩放参与插值，视口来自目标槽。新混合从当前组合相机继续。Scene 暂停默认冻结控制器与混合，配置可允许继续推进。

## 场景切换与重置

切换到不同场景，或以 `SceneReloadMode::Reset` / `Recreate` 重进当前场景时，SceneManager 会在旧场景 `on_exit()` 之后、新场景 `on_enter()` 之前重置目标场景的全部 owned slots。

槽位重置会：

- 清除焦点和世界边界；
- 清除跟随策略、活动运动和效果；
- 清除面向该槽位的未处理请求；
- 将逻辑中心和最终中心归零；
- 将缩放恢复为 `1.0`；
- 保留视口大小。

未声明为 owned 的槽不会因这次切换被破坏。Reuse 当前实例不重置静态姿态，但退出时仍取消活动运动和混合；SceneManager 关闭时重置全部槽位。

## 当前边界

- 相机请求队列仅用于主线程，不提供线程同步。
- 四个相机的初始中心和视口均为零；应用或场景需要显式设置视口。
- 当前不提供动态相机、分屏视口布局、辅助相机占用仲裁或通用多效果栈。
- 旋转和裁剪属于 Camera 与渲染投影层的后续扩展，不应放入跟随策略。
- InputSystem 不会自动把指针位置转换到世界坐标；业务需要按所用槽位显式调用 `screen_to_world`。

## Multi-target framing

`MultiTargetFollowStrategy` frames a group of rectangles and smoothly adjusts both
center and zoom. It does not retain game objects. Submit presentation rectangles
each frame after movement and physics interpolation:

```cpp
#include "engine/camera/multi_target_follow_strategy.h"

// Configure once on scene entry.
auto& cameras = camera_runtime();
cameras.set_follow_strategy(
    elysia::camera::CameraSlot::Main,
    std::make_unique<elysia::camera::MultiTargetFollowStrategy>());

std::optional<elysia::camera::CameraFocus> MyScene::resolve_camera_focus(
    elysia::camera::CameraSlot slot) const
{
    if (slot != elysia::camera::CameraSlot::Main) return std::nullopt;
    const std::array rects{_player->render_rect(), _companion->render_rect()};
    return elysia::camera::make_camera_focus(rects, 0); // Player is primary.
}
```

The same hook can return a different focus for each owned slot.
`CameraFocus::bounds` contains the union; `primary` preserves the selected rectangle.
Empty or entirely invalid lists return `nullopt`. Non-finite rectangles are skipped;
an invalid primary index falls back to the first valid rectangle. Point and line
targets are supported. Direct `set_focus()` rejects non-finite rectangles.

### Configuration

| Setting | Default | Behavior |
| --- | --- | --- |
| `dead_zone_enabled` | `true` | Move only as far as needed to enter the safe region; otherwise follow the group center. |
| `safe_ratio` | `0.70` | Centered safe region as a fraction of viewport width and height. |
| `inner_ratio` | `0.55` | Smaller region that starts the zoom-in delay. |
| `min_zoom`, `max_zoom` | `0.5`, `2.0` | Automatic limits; smaller zoom shows more world. |
| `movement_half_life` | `0.12 s` | Time to halve center error for a fixed destination. |
| `zoom_out_half_life` | `0.10 s` | Fast outward response. |
| `zoom_in_half_life` | `0.35 s` | Slower inward response. |
| `settle_seconds` | `0.4 s` | Delay before zoom-in or leaving primary-only mode. |

Pass `MultiTargetFollowConfig` to the constructor or use `set_config()`, which resets
timers and mode. Ratios are constrained to `0 < inner_ratio < safe_ratio <= 1`.
Zoom limits respect Camera's global range. Invalid half-lives or delay use defaults.

The strategy calculates zoom from the group's width and height in the safe region.
It zooms out immediately but smoothly when needed. Zoom-in starts after the bounds
stay inside the inner region for the settle delay, then continues toward the safe
region fit. New outward demand cancels zoom-in. Movement uses the same frame's
blended zoom. Automatic updates never restart a manual zoom animation.

If the group cannot fit at `min_zoom`, zoom approaches that limit and movement
follows the primary. Group framing resumes once the group can fit inside the inner
region at `min_zoom` for the settle delay. `primary_only()` exposes this state.
A primary larger than the visible region is centered on axes that cannot fit.

### Manual control and lifecycle

```cpp
const auto motion = cameras.move_to(
    elysia::camera::CameraSlot::Main, {.zoom = 1.5f}, 1.0);
```

An active pose motion owns the camera pose through the final frame; omitted target
channels hold their value from the beginning of that segment.
Tracking and automatic zoom resume on the next frame for ResumeFollow motions.
Immediate `set_zoom()` applies at once, with automatic zoom resuming next update.
Values outside the automatic range return smoothly rather than snapping.

Initial acquisition and reacquisition are smooth. Missing focus holds framing and
resets timers; explicit manual effects still run. Replacing a strategy resets state.
Explicit snap requests still snap the center to the group bounds. Legacy strategies
retain immediate first acquisition and do not change zoom.

`IFollowStrategy::update()` is non-const and returns `CameraFollowResult` with center
and optional zoom. Custom strategies must migrate from `update_center()`.
`automatic_zoom_enabled` remains available to strategies when no pose motion owns the camera.
`reset()` clears state; `snap_on_acquisition()` defaults to true, while the new
strategy overrides it to false.

Smooth tracking can temporarily leave targets outside the screen after rapid
separation or teleportation. World bounds take priority; shake is applied after
framing. Neither guarantees every target remains visible. Invalid viewports or
non-positive/non-finite time steps do not advance the new strategy.

### Demo and checks

Open **Demo Gallery > Multi-target Camera**. The white outline marks the primary.
WASD moves the primary; Q/E shifts the other block horizontally. Buttons toggle
DeadZone, primary selection, automatic separation/reunion and world bounds, and
provide teleport, manual zoom, reset and return. Blue outlines mark the safe region,
green the inner region, gray the group bounds, and red optional world bounds.
The HUD shows zoom and group/primary-only mode. No ImGui dependency is required.

Run `ctest --test-dir out/build/sdl3-Debug -L camera --output-on-failure`.
`demo_scene_tests` checks demo controls, return routing and re-entry. Set
`ELYSIA_CAMERA_QA_DIR` to export rendered checkpoints as PNG files for inspection.
