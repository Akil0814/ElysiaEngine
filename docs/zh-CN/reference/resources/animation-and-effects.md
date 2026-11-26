# 动画与特效 JSON

本文说明动画、图集与特效定义如何从配置生成注册请求。特效引用已经存在的动画，不会通过一个特效键自动创建图片或动画资源。

`Animation` 负责 `Atlas` 与 `Animation` 资源。EffectDefinition 加载只将已存在的 `Animation` 注册为定义，不会单独请求图片或创建动画。核心资源清单和附加内容模块最终进入同一请求计划与资源注册表。

运行时 `EffectService` 还支持纯色和静态图片屏幕特效；图片通过已加载的 texture 资源键查询，不新增 EffectDefinition 配置。屏幕特效由 `EffectManager` 内的独立运行时管理，使用逻辑视口坐标，在 UI 前或 UI 后绘制。所有实例属于当前场景，场景解绑时清理，不跨场景保留；内容清理先释放特效，再释放其借用的纹理。

运行时特效请求使用 `std::expected` 返回成功结果或 `EffectRequestFailure`。屏幕特效与粒子特效的成功值是各自的句柄，世界动画与浮动数字的成功值为 void；请求错误与加载时的 `EffectRegistrationFailure` 分开。请求结果的处理方式见[动画与特效指南](../../user-guide/systems/animation-and-effects.md)。

依赖查询与阶段终止是两个独立事件：`find_atlas()` / `find_definition()` 查询失败时的 Warn 记录缺失依赖；随后 `Animation` 或 EffectDefinition 注册终止时的 Error 记录加载阶段失败。两条日志不得合并或按重复日志删除。

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
      "loop": true,
      "horizontal_strip": true
    }
  ]
}
```

每项必须有合法 dotted `key`、相对于 `assets/textures/` 的 `path`、正整数 `frame_count`、正数 `fps` 和 boolean `loop`。`horizontal_strip` 可省略，默认 `false`。

- 目录帧：`path` 必须是目录，且必须提供非空 `frame_prefix`。
- 横向序列图：`path` 必须是普通图片，不能出现 `frame_prefix`。

## 实体 Animation 能力声明

以下是当前 `ryougi_sample` 模块中 `capabilities` 的动画部分；完整模块与实体入口见[当前仓库样例](entity-content.md#当前仓库样例)。此片段不能直接替代完整模块根结构。

```json
{
  "animations": {
    "texture_root": "textures/examples/ryougi/{id}",
    "config_template": "configs/examples/ryougi/{id}/animation_info.json",
    "frame_prefix_template": "{id}_{animation}{segment_suffix}",
    "layouts": {
      "fighter": "configs/examples/ryougi/animation_layout.json"
    }
  }
}
```

`texture_root`、`config_template` 和非空 `layouts` 必填。每个实体用 `animation_layout` 选择布局，再由自己的动画配置描述帧数和播放参数。模板及路径规则见 [实体内容模块](entity-content.md)。

## 动画布局与配置

布局只描述相对于实体纹理根目录的路径：

```json
{
  "animations": {
    "idle": { "path": "idle" },
    "attack_normal": { "segment_path": "animation/attack/normal/{segment}" }
  }
}
```

一个条目只能有 `path` 或 `segment_path`。`segment_path` 中的 `{segment}` 会替换为两位文件系统编号；没有该标记时也会自动追加对应两位目录。

每个实体动画配置只允许 `defaults`、`animations`：

```json
{
  "defaults": { "source_type": "frame_directory" },
  "animations": {
    "idle": { "frame_count": 7, "fps": 10, "loop": true },
    "attack_normal": {
      "segments": [
        { "frame_count": 7, "fps": 10, "loop": false },
        { "frame_count": 8, "fps": 10, "loop": false }
      ]
    }
  }
}
```

`source_type` 必填，只能是 `frame_directory` 或 `horizontal_strip`，并作用于整个配置；不支持单项覆盖。普通动画和每个分段都必须提供正数 `frame_count`、正数 `fps` 与 boolean `loop`。动画逻辑名必须存在于选中的布局。

## 帧来源与分段

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

分段的运行时资源键不补位，文件系统编号补两位：

```text
segment index            0                         1
runtime key              RyougiShiki.attack_normal.0 RyougiShiki.attack_normal.1
layout directory         .../00                    .../01
segment suffix           _00                        _01
first frame              ..._00_000.png             ..._01_000.png
```

分段索引范围是 `0–99`；帧索引从 0 开始，文件名使用至少三位数字。

## 特效

核心 `effects_manifest.json` 将已有 `Animation` 资源键映射为 EffectDefinition：

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

下面两段是假设项目的特效配置示例，需要项目自行提供对应配置、动画和素材。当前 `ryougi_sample` 没有声明 `effects` 能力。实体模块的 `effects` 能力声明只提供每实体配置路径：

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

特效名可与动画名不同。映射分段动画时，会为已配置的每个分段生成一个 EffectDefinition；若目标动画不存在则配置失败。宽高必须同时为零/省略，或同时为正数；零表示播放时使用动画单帧自然尺寸。

若项目自行配置名为 `character_effects`、命名空间为 `effect` 的模块，资源键可以是 `RyougiShiki.effect.attack_normal.0`，文件前缀可以由模块模板配置为 `RyougiShiki_effects_attack_normal_00`。这些是配置结果示例，不是当前 `ryougi_sample` 默认提供的特效键；相应特效配置与素材需要项目自行提供。加载时先完成 `Animation` 注册，再注册 EffectDefinition。

<a id="particle-effect-manifest"></a>
## 粒子预设清单

现有 `effects_manifest.json` 中的 `effects` 数组保存动画特效，始终必填，可以为空。粒子放在可选的 `particle_effects` 数组中。每项必填 `key` 和 `texture_key`，分别用于播放请求和引用加载计划中的纹理资源。粒子键在粒子注册表内必须唯一。

启动和内容重载会在纹理加载完成后注册粒子预设。此时会校验源矩形是否位于纹理内。配置失败会提供清单路径和对应的 JSON Pointer。播放方式见[动画与特效指南](../../user-guide/systems/animation-and-effects.md)。

| 可选字段 | 格式与默认值 |
| --- | --- |
| `max_particles`、`burst_count` | 非负整数，默认 `1024` 和 `32`。容量必须大于零。 |
| `emission_rate`、`emission_duration_seconds` | 默认每秒发射 `0` 个，发射时长默认省略。设置时长时必须为正。 |
| `velocity_min`、`velocity_max`、`acceleration` | `[x, y]`，默认均为 `[0, 0]`。 |
| `lifetime_seconds`、`angle_degrees`、`angular_velocity_degrees` | `{ "min": ..., "max": ... }`，默认范围分别为 `0.4–0.8`、`0–0`、`0–0`。 |
| `start_size`、`end_size` | `[width, height]`，默认 `[8, 8]` 和 `[0, 0]`。 |
| `start_color`、`end_color` | `[r, g, b, a]`，各通道为 `0–255` 整数，默认 `[255, 255, 255, 255]` 和 `[255, 255, 255, 0]`。 |
| `source_rect` | 像素坐标 `[x, y, width, height]`，省略时使用整张纹理。 |
| `layer` | 默认 `"effect_front"`。可选 `background`、`terrain`、`effect_back`、`item`、`character`、`effect_front`、`foreground`。 |
| `blend` | 默认 `"alpha"`，也支持 `"add"`。 |

## 资源键冲突

核心与所有模块的请求会按 `Atlas`、`Animation`、`Effect`、`ParticleEffect`、Texture、Font、Sound、Music 注册表分别去重。同一字符串可出现在不同注册表；同一注册表冲突会在提交前失败，并在诊断中提供首次与重复声明的项目相对路径、JSON 路径指针、来源范围、模块、能力声明、实体、逻辑名和分段。
