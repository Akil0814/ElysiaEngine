# 动画与特效 JSON

Animation 负责 Atlas 与 Animation 资源。Effect 只将已存在的 Animation 注册为 EffectDefinition，不会单独请求图片或创建动画。核心 manifest 和 additional module 最终进入同一请求计划与资源 registry。

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

`texture_root`、`config_template` 和非空 `layouts` 必填。每个实体用 `animation_layout` 选择 layout，再由自己的 animation config 描述帧数和播放参数。模板及路径规则见 [实体内容 module](entity-content.md)。

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

当前角色特效使用 `character_effects` module，资源 key 例如 `RyougiShiki.effect.attack_normal.0`，文件前缀由 module template 配置为 `RyougiShiki_effects_attack_normal_00`。Animation 先完成注册，EffectDefinition 最后注册。

## key 冲突

核心与所有 module 的请求会按 Atlas、Animation、Effect、Texture、Font、Sound、Music registry 分别去重。同一字符串可出现在不同 registry；同一 registry 冲突会在提交前失败，并在诊断中提供 first/second 的项目相对路径、JSON pointer、scope、module、capability、entity、逻辑名和 segment。
