# 实体内容 module

`manifests.additional` 的每项都是一个任意命名的实体资源包。所有包由同一个 loader 处理；`characters`、`character_effects` 和 `enemies` 只是当前配置的名称，不是代码白名单。

## Module manifest

```json
{
  "entities": "configs/character/characters_manifest.json",
  "key_namespace": "effect",
  "capabilities": {
    "animations": {
      "texture_root": "textures/character/{id}",
      "config_template": "configs/character/{id}/effect_animation_info.json",
      "frame_prefix_template": "{id}_effects_{animation}{segment_suffix}",
      "layouts": {
        "fighter": "configs/character/layouts/character_effect_animation_layout.json"
      }
    },
    "effects": {
      "config_template": "configs/character/{id}/effect_info.json"
    }
  }
}
```

根对象只能包含 `entities`、`key_namespace`、`capabilities`。三项都必填；`key_namespace` 可以是空字符串，`capabilities` 可以为空对象。允许的 capability 固定为可选的 `animations`、`effects`、`textures`、`audio`；未知字段或 capability 会失败。`effects` 存在时，同一 module 必须同时提供 `animations`。

## Capability

`animations` 必须包含 `texture_root`、`config_template` 和非空 `layouts`。`config_template` 必须含 `{id}`。若该实体动画配置的 `source_type` 是 `frame_directory`，还必须提供 `frame_prefix_template`；横向序列图不使用此前缀。

`frame_prefix_template` 只允许 `{id}`、`{animation}`、`{segment_suffix}`，必须含前两项；配置存在分段动画时还必须含 `{segment_suffix}`。它只能是文件名前缀，不能包含路径分隔符或 `..`。

```json
{
  "textures": {
    "texture_root": "textures/character/{id}",
    "layout": "configs/character/layouts/character_texture_layout.json"
  },
  "audio": {
    "audio_root": "audio/character/{id}",
    "layout": "configs/character/layouts/character_audio_layout.json"
  }
}
```

Texture 和 Audio capability 都要求根目录和 layout。Texture layout 可指向文件或非递归目录；目录中的直接文件按路径排序，文件 stem 参与资源 key。Audio layout 的目标必须是普通文件，当前只生成 Sound。

## `{id}` 与路径解析

`texture_root` 与 `audio_root` 中出现 `{id}` 时，会替换全部标记；未出现时自动在末尾追加实体 id。只允许此 token，实体化后的根目录必须存在。

所有 `config_template` 必须显式包含 `{id}`，替换后必须是存在的普通配置文件。Animation 的 `layouts` 与 Texture/Audio 的 `layout` 都相对于 `assets/` 解析；layout 内的资源路径则相对于实体化后的对应根目录解析。

## Entity manifest

```json
{
  "entities": [
    {
      "id": "FlyingDemon",
      "enabled": true,
      "animation_layout": "normal"
    }
  ]
}
```

| 字段 | 规则 |
| --- | --- |
| `id` | 必填、唯一、合法 key component；同时是资源目录标识、模板参数和运行时资源 key 的首段。当前实体采用 PascalCase 约定。 |
| `enabled` | 可选，默认 `true`；为 `false` 时跳过该实体。 |
| `animation_layout` | 使用 Animation capability 时必填，且必须匹配该 capability 的 `layouts` 键；若提供也必须是合法 component。 |

Entity 条目只能包含上述字段。`id` 的语法仍是通用的 `[A-Za-z0-9_]+`，PascalCase 是仓库实体命名约定而非额外语法限制。角色显示名继续由独立的 i18n `display_name_key` 控制，不随实体 id 改写。

## 资源 key

所有 capability 使用同一个 key builder：

```text
base      = <entity id>[.<key_namespace>]
Animation = <base>.<animation>[.<segment index>]
Effect    = <base>.<effect>[.<segment index>]
Texture   = <base>.<texture logical name>[.<directory file stem>]
Sound     = <base>.<audio logical name>
```

例如：

```text
characters / namespace ""      -> RyougiShiki.idle
character_effects / "effect"   -> RyougiShiki.effect.attack_normal.0
```

每个 component 使用 `[A-Za-z0-9_]+`；点仅用于连接 component。空 component、横线、空格、非 ASCII 字符和连续/首尾点都非法。segment key 永不补位；补两位只属于文件系统的目录与文件名前缀。

## 当前 module 划分

| module | namespace | capability |
| --- | --- | --- |
| `characters` | `""` | `animations`、`textures`、`audio` |
| `character_effects` | `effect` | `animations`、`effects` |
| `enemies` | `""` | `animations` |

同一个 entity manifest 可以被多个 module 复用。角色特效 module 只在 `effect` namespace 下加载自身动画，并把已有动画注册为 EffectDefinition；它不加载角色本体动画。

## 常见失败

- entity 缺少 id、id 重复、含未知字段或 component 非法；
- 模板缺少 `{id}`、使用未知 token，或解析后的路径不存在；
- `effects` 没有同 module 的 `animations`；
- entity 的 `animation_layout` 不在 capability 的 `layouts` 中；
- Texture/Audio layout 或目录纹理 stem 非法；
- 核心和 module 在同一资源 registry 生成重复 key。错误会列出 first/second 两个完整 `ResourceOrigin`。
