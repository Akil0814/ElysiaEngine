# 动画与特效

先在[资源配置](../content/resources.md)中声明图集、动画和特效，再等待内容加载成功。AnimationService 创建独立播放器；EffectService 把特效对象加入当前活动场景。游戏无需初始化或关闭这两个服务。

## 播放对象动画

以下类可放入游戏头文件，在场景进入、资源加载成功后创建。test.animation 对应下文示例清单，图片需由项目提供。

```cpp
#include "engine/animation/animation_service.h"
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/tools/logger.h"

class AnimatedItem final : public elysia::core::GameObject,
                           public elysia::core::Updatable {
public:
    AnimatedItem() : GameObject(elysia::core::DepthLayer::Item) {
        set_world_rect({100, 100, 64, 64});
        animation_ = ELYSIA_ANIMATIONS->create_animation("test.animation");
        if (!animation_) {
            ELYSIA_LOG_ERROR("animation", "Missing test.animation");
            destroy();
        }
    }
    void update(double delta) override {
        if (animation_) animation_->update(scaled_delta(delta));
    }
    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out) const override {
        if (!animation_) return;
        elysia::core::RenderCommand command;
        if (animation_->build_render_command(render_rect(), 0.0,
                elysia::core::SpriteFlip::None, command))
            out.push_back(command);
    }
private:
    std::unique_ptr<elysia::animation::Animation> animation_;
};
```

播放器归调用方所有，但内部 Atlas 和帧纹理为借用。资源重载前销毁或停用依赖旧资源的播放器；暂停播放器并不能让借用资源跨重载有效。

`set_loop` 设置循环，`set_interval_seconds` 设置帧间隔，`pause`/`resume` 暂停或继续，`reset` 从首帧重新开始并恢复计时。非循环播放结束时 is_finished 为 true，停留最后一帧；resume 不替代 reset。`set_on_finished` 接收无参数回调，回调中不要立即释放正在执行 update 的播放器。

`current_frame` 和服务的 `find_definition` 都返回可空借用。`append_render_commands` 可附加颜色覆盖；返回 false 时不假设生成了命令，覆盖需要相应帧的 coverage mask。通常使用上面的 build_render_command 即可。

## 请求场景特效

以下辅助函数在活动场景的安全阶段调用，例如派生场景完成基类更新后处理本帧积累的表现请求。不要在对象列表遍历、碰撞监听或特效回调中直接向同一场景插入新特效。

```cpp
#include "engine/effects/effect_service.h"
#include "engine/tools/logger.h"

void show_hit(elysia::core::Vector2 position) {
    elysia::effects::AnimationEffectSpawnRequest request;
    request.effect_key = "effect.test";
    request.position = position;
    request.anchor = elysia::effects::EffectAnchor::Center;
    if (!ELYSIA_EFFECTS->request_animation_effect(request))
        ELYSIA_LOG_WARN("effects", "Hit effect was not created.");

    elysia::effects::FloatingNumberEffectSpawnRequest number;
    number.text = "120";
    number.position = position;
    number.target_height = 20.0f;
    number.lifetime_seconds = 0.6;
    if (!ELYSIA_EFFECTS->request_floating_number_effect(number))
        ELYSIA_LOG_WARN("effects", "Damage number was not created.");
}
```

动画特效 position 是选定 anchor 的世界位置；可覆盖 size、angle_degrees、flip，设置 start_delay_seconds。缺少活动场景、定义或创建失败返回 false。该接口立即加入对象，true 不表示已经播放结束。

特效非循环动画完成后调用 on_finished 并标记销毁；循环动画不会自然结束。服务不返回可取消句柄。需要主动管理的长期表现优先由自己的对象持有 Animation；不要假设退出场景会触发完成回调。场景缓存复用时，特效可能仍保留在原场景中。

on_started、on_finished 和 scheduled_callbacks 的回调参数是 AnimationEffect&；scheduled_callbacks 的 delay_seconds 相对于播放时间。回调捕获的数据必须覆盖实际执行期，不能把完成回调作为释放外部资源的唯一途径。

浮动数字不需要配置动画清单，但需要有效字体与渲染依赖。text 支持 `0–9`、`-`、`.`、`/`、`%`，不支持加号或普通文字；普通文字使用 UI 文本。position、字号高度、时间必须合法，lifetime_seconds 和 target_height 为正，time_scale 非负。effects 可组合线性/弧线 motion、scale、fade，其 time_range 使用 0–1 生命周期进度。没有设置这些效果时仍按寿命结束并销毁。其 on_finished 参数为 FloatingNumberEffect&。

## 全屏特效

`EffectService` 支持纯色和静态图片遮罩。它们使用游戏逻辑视口的屏幕坐标，不受相机移动或缩放影响，也不覆盖窗口的留黑区域。它们全部属于当前活动场景：退出场景（包括缓存复用）、故障恢复、关闭或内容清理时移除，句柄失效；不支持跨场景保留。

```cpp
#include "engine/effects/effect_service.h"

// 闪白：立即显示，然后在 0.2 秒内淡出。
std::optional<elysia::effects::ScreenEffectHandle> flash_white() {
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {255, 255, 255};
    request.playback.hold_seconds = 0.0;
    request.playback.fade_out_seconds = 0.2;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}

// 图片必须已通过项目的纹理资源配置加载。
std::optional<elysia::effects::ScreenEffectHandle> show_screen_image() {
    elysia::effects::ScreenImageEffectRequest request;
    request.texture_key = "screen.blood"; // 项目提供的纹理 key
    request.fit = elysia::effects::ScreenEffectFit::Cover;
    request.playback.fade_in_seconds = 0.1;
    request.playback.hold_seconds = 0.3;
    request.playback.fade_out_seconds = 0.4;
    request.playback.layer = elysia::effects::ScreenEffectLayer::BeforeUi;
    return ELYSIA_EFFECTS->request_screen_image_effect(request);
}
```

纯色请求与图片请求共享 `ScreenEffectPlayback`。默认目标透明度为 1，淡入/淡出为 0 秒，保持 0.6 秒，结束策略为 `Timed`，层级为 `AfterUi`，时钟为 `Unscaled`。透明度从 0 线性淡入到目标值，保持结束后线性淡出；淡入为 0 时创建即使用目标透明度。颜色 alpha、图片透明通道与播放透明度相乘。

`Timed` 的保持时间从淡入完成后开始；`Manual` 淡入后持续保持，直到主动结束。`stop_screen_effect(handle)` 从当前透明度开始，使用请求中的淡出时长；为 0 时立即移除。已经淡出的特效重复 stop 返回 false，不重启淡出。`cancel_screen_effect(handle)` 立即移除。`is_screen_effect_active(handle)` 查询是否存在，不代表淡入是否完成。失效句柄安全返回 false，不能控制复用槽位的新特效。

`Unscaled` 使用原始帧时间，不受时间缩放和场景暂停影响。`Scene` 使用缩放后时间，并跟随场景暂停。每帧先推进已有屏幕特效，再更新场景；在本帧输入、场景进入或更新中创建的特效从下一帧开始计时。请求不调用用户回调，可在场景对象更新中创建或取消。

`BeforeUi` 位于世界和调试图形之后、UI 之前；`AfterUi` 覆盖 UI。开发工具面板在两者之后绘制。同一层内按创建顺序叠加。图片默认 `Stretch` 拉伸铺满；`Cover` 等比居中铺满并裁切；`Contain` 等比居中完整显示，未覆盖区域保持透明。

创建返回 optional 句柄。没有活动场景、纹理缺失、时间为负或非有限、透明度不在 0–1、定时请求的三个时长全为 0 时返回空值并记录日志。图片借用已加载纹理，资源重载前必须通过现有内容清理流程清除特效；直接释放仍被引用的纹理不受支持。本次不支持帧动画和后处理。

### 两个场景分别完成黑幕转场

旧场景请求渐黑，并用与特效相同的时钟累计时间；达到淡入时长后按现有场景路由接口请求切换。使用 `Manual` 让黑幕在切换前持续保持。创建失败时不要启动转场计时。

```cpp
std::optional<elysia::effects::ScreenEffectHandle> begin_exit_blackout() {
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {0, 0, 0};
    request.playback.fade_in_seconds = 0.4;
    request.playback.end = elysia::effects::ScreenEffectEnd::Manual;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}

// 在新场景 on_enter 中调用，在首次绘制前创建全黑遮罩。
std::optional<elysia::effects::ScreenEffectHandle> begin_enter_blackout() {
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {0, 0, 0};
    request.playback.hold_seconds = 0.0;
    request.playback.fade_out_seconds = 0.4;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}
```

默认时钟下，旧场景从下一次更新开始累计 `ELYSIA_TIME->raw_delta()`，累计到 0.4 秒时发出切换请求。退出旧场景会清理旧遮罩；新场景进入时创建的新遮罩从全黑开始淡出。因此新场景第一帧仍为黑色，不需要跨场景句柄。场景路由和转场计时由游戏负责。

### 在游戏中手动测试

主菜单 → 演示画廊 → 引擎功能实验室，顶部的 Screen effects 面板支持鼠标按钮和快捷键：

| 按键 | 效果 |
|---|---|
| F1 | 闪白并自动淡出 |
| F2 / F3 | 渐黑后自动恢复 / 渐黑后持续保持 |
| F4 / F5 | 从当前透明度淡出 / 立即取消 |
| F6 / F7 / F8 | 图片 Stretch / Cover / Contain |
| F9 | 切换 UI 前后层级，清除当前预览后重新选择效果 |

面板显示 Playing、Finished 或请求失败状态。每次选择效果会替换上一次预览。黑屏遮住按钮时仍可用快捷键；Esc 优先清除正在播放的效果，没有活动效果时才返回画廊。图片使用已加载的 `demo.screen_effect` 测试纹理；其 3:2 网格、四角标记和圆环用于观察拉伸、裁切、等比显示和透明叠加。

## 资源配置

下文是项目需提供的 JSON 格式；核心 manifest 路径由 content_registry 的 required 项指定。实体资源包的模板、布局与 key 规则见本目录[资源指南](../content/resources.md)。
## 核心动画

`assets/configs/manifests/animations_manifest.json`：

```json
{
  "animations": [
    {
      "key": "test.animation",
      "path": "test/frame_group.png",
      "frame_count": 14,
      "fps": 10,
      "loop": false,
      "horizontal_strip": true
    }
  ]
}
```

每项必须有合法 dotted `key`、相对于 `assets/textures/` 的 `path`、正整数 `frame_count`、正数 `fps` 和 boolean `loop`。`horizontal_strip` 可省略，默认 `false`。

- 目录帧：`path` 必须是目录，且必须提供非空 `frame_prefix`。
- 横向序列图：`path` 必须是普通图片，不能出现 `frame_prefix`。

## Entity Animation capability

```json
{
  "animations": {
    "texture_root": "textures/character/{id}",
    "config_template": "configs/character/{id}/animation_info.json",
    "frame_prefix_template": "{id}_{animation}{segment_suffix}",
    "layouts": {
      "fighter": "configs/character/layouts/character_animation_layout.json"
    }
  }
}
```

`texture_root`、`config_template` 和非空 `layouts` 必填。每个实体用 `animation_layout` 选择 layout，再由自己的 animation config 描述帧数和播放参数。模板及路径规则见 [实体内容资源包](../content/resources.md)。

## Animation layout 与 config

layout 只描述相对于实体纹理根目录的路径：

```json
{
  "animations": {
    "idle": { "path": "idle" },
    "attack_normal": { "segment_path": "animation/attack/normal/{segment}" }
  }
}
```

一个条目只能有 `path` 或 `segment_path`。`segment_path` 中的 `{segment}` 会替换为两位文件系统编号；没有该 token 时也会自动追加对应两位目录。

每个实体 animation config 只允许 `defaults`、`animations`：

```json
{
  "defaults": { "source_type": "frame_directory" },
  "animations": {
    "idle": { "frame_count": 7, "fps": 10, "loop": false },
    "attack_normal": {
      "segments": [
        { "frame_count": 7, "fps": 10, "loop": false },
        { "frame_count": 8, "fps": 10, "loop": false }
      ]
    }
  }
}
```

`source_type` 必填，只能是 `frame_directory` 或 `horizontal_strip`，并作用于整个 config；不支持单项覆盖。普通动画和每个 segment 都必须提供正数 `frame_count`、正数 `fps` 与 boolean `loop`。动画逻辑名必须存在于选中的 layout。

## 帧来源与 segment

目录帧严格由配置生成，不扫描 PNG：

```text
<source>/<prefix>_000.png
<source>/<prefix>_001.png
...
```

缺少任一预期帧会失败；额外 PNG 不读取。横向图固定为单行、从左到右、等宽、无边距和无帧间距：

```text
<texture_root>/<resolved layout path>/<animation>.png
frame_width = image_width / frame_count
source_rect = { index * frame_width, 0, frame_width, image_height }
```

横向图只解码一份 texture。图片宽度必须能被 `frame_count` 整除，帧宽和图片高度都必须大于零。

segment 的运行时 key 不补位，文件系统编号补两位：

```text
segment index            0                         1
runtime key              RyougiShiki.attack_normal.0 RyougiShiki.attack_normal.1
layout directory         .../00                    .../01
segment suffix           _00                        _01
first frame              ..._00_000.png             ..._01_000.png
```

segment index 范围是 `0–99`；帧索引从 0 开始，文件名使用至少三位数字。

## 特效

核心 `effects_manifest.json` 将已有 Animation key 映射为 EffectDefinition：

```json
{
  "effects": [
    {
      "key": "effect.test",
      "animation_key": "test.animation",
      "default_width": 128,
      "default_height": 128,
      "default_angle_degrees": 0
    }
  ]
}
```

Entity module 的 `effects` capability 只提供每实体配置路径：

```json
{
  "effects": {
    "config_template": "configs/character/{id}/effect_info.json"
  }
}
```

`effect_info.json` 只描述逻辑映射：

```json
{
  "effects": {
    "slash_trail": {
      "animation": "attack_normal",
      "default_width": 0,
      "default_height": 0,
      "default_angle_degrees": 0
    }
  }
}
```

effect 名可与 animation 名不同。映射分段动画时，会为已配置的每个 segment 生成一个 EffectDefinition；若目标 animation 不存在则配置失败。宽高必须同时为零/省略，或同时为正数；零表示播放时使用动画单帧自然尺寸。


## key 冲突

核心与所有 module 的请求会按 Atlas、Animation、Effect、Texture、Font、Sound、Music registry 分别去重。同一字符串可出现在不同 registry；同一 registry 冲突会在提交前失败，并在诊断中提供 first/second 的项目相对路径、JSON pointer、scope、module、capability、entity、逻辑名和 segment。

## 常见误用与参考

动画不会因为创建就自动更新；由所属 Updatable 每帧驱动一次。特效已经是场景对象，不再手动 update。不要在绘制时推进动画，不在资源失效后继续使用旧 Atlas，也不要用动画完成回调决定必须发生的游戏规则结果。

- [动画接口](../../../../engine/animation/animation.h)、[特效请求参数](../../../../engine/effects/effect_types.h)
- [场景生命周期](../scene/scene.md)、[时间](time-and-timers.md)、[返回使用指南](../README.md)
