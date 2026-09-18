# Camera 工作流

## 模块结构

相机模块由三个层次组成：

```text
CameraManager（全局唯一拥有者和写入口）
  ├─ Main       Camera + CameraController
  ├─ Cinematic  Camera + CameraController
  ├─ Auxiliary1 Camera + CameraController
  └─ Auxiliary2 Camera + CameraController
```

- `Camera` 保存最终渲染中心、屏幕视口大小和缩放倍率，并负责世界坐标与屏幕坐标之间的转换。
- `CameraController` 保存逻辑中心、焦点、世界边界、跟随策略、震屏和变焦过渡。
- `CameraManager` 固定拥有四组相机，不提供动态创建或销毁接口。

四个槽位定义如下：

| 槽位 | 用途 |
| --- | --- |
| `Main` | Scene 默认使用的世界相机。 |
| `Cinematic` | 过场和演出视角。 |
| `Auxiliary1` | 无预设用途的扩展相机。 |
| `Auxiliary2` | 无预设用途的扩展相机。 |

## 访问规则

`CameraManager` 是受管相机的唯一配置入口。外部只能取得 `const Camera&`，用于渲染、查询可见区域和坐标投影：

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

配置接口立即生效，包括中心、视口、缩放、焦点、边界和跟随策略。直接设置中心时，Manager 会同步 Controller 的逻辑中心和 Camera 的最终中心，避免两者失配。直接设置缩放会取消该槽位尚未处理或正在运行的平滑变焦，并按新的世界可见范围重新限制中心。

## 视口、缩放与坐标

`viewport_size` 使用屏幕像素，`zoom` 默认为 `1.0`，合法范围为 `[0.1, 10.0]`。普通越界值会被钳制，非有限值回退为 `1.0`。相机在世界中实际可见的尺寸为：

```text
world_viewport_size = viewport_size / zoom
```

`world_to_screen` 和 `screen_to_world` 同时支持点和矩形。矩形转换会同时变换位置与尺寸；屏幕坐标是 viewport-local 坐标，不含窗口位置或 UI 布局偏移。UI 命令不经过这些转换。

## 请求与更新时序

震屏、平滑变焦、立即对焦和清除效果是瞬时请求：

```cpp
cameras->request_shake(CameraSlot::Main, shake_params);
cameras->request_zoom_to(CameraSlot::Main, 2.0f, 0.5);
cameras->request_snap_to_focus(CameraSlot::Cinematic);
cameras->request_clear_effects(CameraSlot::Main);
```

请求按提交顺序进入单线程 FIFO 队列。`CameraManager::update(delta)` 的执行顺序为：

1. 按 FIFO 顺序处理所有待执行请求。
2. 更新四个 CameraController。
3. Controller 使用 Smoothstep 更新手动平滑变焦，该帧暂停策略的自动缩放。
4. Controller 应用策略返回的中心与可选 zoom，然后按更新后的倍率限制世界边界。
5. Controller 叠加震屏偏移。
6. 最终中心写回 Camera，供随后渲染使用。

同一相机保存一个活动震屏和一个独立的平滑变焦，因此两者可以并行。新的震屏替换旧震屏，新的变焦从当前倍率接续并替换旧变焦。持续时间小于或等于零的变焦立即完成。清除效果会停止震屏和变焦，但保留停止瞬间的倍率与逻辑中心。请求严格遵循 FIFO 顺序。

DeadZone 使用 viewport-local 屏幕像素定义。焦点会先按当前 zoom 投影后再与死区比较，因此改变倍率不会改变死区在屏幕上的视觉大小。

## Scene 集成

Scene 不再拥有 Camera 或 CameraController。基础场景行为为：

- `Scene::on_update` 每帧将 `resolve_camera_focus()` 的结果写入 `Main`，然后更新 CameraManager 中的全部相机。新入口默认包装原来的 `resolve_camera_focus_rect()`。
- 场景内部渲染流程默认使用 `Main` 投影世界渲染命令。`Scene::on_render` 是由 `SceneManager` 调用的私有非虚函数；游戏对象和 UI 通过提交引擎绘制命令参与渲染，游戏场景不重写渲染入口。
- UI 命令仍直接使用屏幕坐标执行，不经过世界相机。
- Scene 子类可以通过受保护的 `set_render_camera_slot()` 改用 `Cinematic`、`Auxiliary1` 或 `Auxiliary2` 渲染世界。

只有 Main 会自动接收 Scene 的焦点矩形。其他三个槽位的焦点和配置完全由业务代码管理，不会被 Scene 基础更新覆盖。

```cpp
std::optional<elysia::core::Rect> MyScene::resolve_camera_focus_rect() const
{
    return _player ? std::optional(_player->world_rect()) : std::nullopt;
}
```

场景若需要使用演出相机作为世界渲染相机，可以在进入时选择槽位：

```cpp
set_render_camera_slot(elysia::camera::CameraSlot::Cinematic);
```

## 场景切换与重置

切换到不同场景，或以 `SceneReloadMode::Reset` 重进当前场景时，SceneManager 会在旧场景 `on_exit()` 之后、新场景 `on_enter()` 之前重置 Main。

Main 重置会：

- 清除焦点和世界边界；
- 清除跟随策略和活动效果；
- 清除面向 Main 的未处理请求；
- 将逻辑中心和最终中心归零；
- 将缩放恢复为 `1.0`；
- 保留视口大小。

`Cinematic`、`Auxiliary1` 和 `Auxiliary2` 跨场景保留。使用这些槽位的业务负责主动重新配置或调用 `CameraManager::reset(slot)`。Reuse 当前活动场景不会触发重置；SceneManager 关闭时会重置 Main。

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
auto* cameras = elysia::camera::CameraManager::instance();
cameras->set_follow_strategy(
    elysia::camera::CameraSlot::Main,
    std::make_unique<elysia::camera::MultiTargetFollowStrategy>());

// Override the new scene hook. The existing single-rectangle hook still works.
std::optional<elysia::camera::CameraFocus> MyScene::resolve_camera_focus() const
{
    const std::array rects{_player->render_rect(), _companion->render_rect()};
    return elysia::camera::make_camera_focus(rects, 0); // Player is primary.
}
```

For other slots, pass the helper result to `CameraManager::set_focus()`.
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
cameras->request_zoom_to(elysia::camera::CameraSlot::Main, 1.5f, 1.0);
```

An active manual transition owns zoom through its final frame. Tracking continues
using actual zoom; automatic zoom and its inward timer resume on the next frame.
Immediate `set_zoom()` applies at once, with automatic zoom resuming next update.
Values outside the automatic range return smoothly rather than snapping.

Initial acquisition and reacquisition are smooth. Missing focus holds framing and
resets timers; explicit manual effects still run. Replacing a strategy resets state.
Explicit snap requests still snap the center to the group bounds. Legacy strategies
retain immediate first acquisition and do not change zoom.

`IFollowStrategy::update()` is non-const and returns `CameraFollowResult` with center
and optional zoom. Custom strategies must migrate from `update_center()`.
`automatic_zoom_enabled` in the context is false during manual transitions.
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
