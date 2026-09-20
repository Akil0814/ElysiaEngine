# 资源加载与使用

资源由应用和内容加载流程统一加载。游戏代码使用 `ELYSIA_RESOURCES` 查询已加载的资源，不自行初始化或关闭资源管理器。先完成[游戏初始化](../game-initialization.md)，通过[启动加载场景](../builtin-scenes/startup-loading.md)加载内容，成功后再进入使用这些资源的场景。

## 最小使用流程

1. 在下文对应 manifest 中声明资源，将文件放在规定的资源根目录。
2. 确保 `assets/content_registry.json` 引用这些 manifest；重新开始内容加载后才会读取修改。
3. 加载成功后按 key 查询并处理空结果。纹理交给[游戏对象](../scene/game-objects.md)绘制，音频交给[音频服务](../systems/audio.md)播放。

下面的辅助函数放在游戏代码中，在内容加载成功后调用；调用方决定缺少图片时跳过显示还是进入错误流程。

```cpp
#include "engine/resources/resource_service.h"
#include "engine/tools/logger.h"

SDL_Texture* find_icon() {
    auto* texture = ELYSIA_RESOURCES->find_texture("ui.moon");
    if (!texture)
        ELYSIA_LOG_ERROR("resources", "Missing texture: ui.moon");
    return texture; // 借用，不调用 SDL_DestroyTexture。
}
```

| 查询 | 返回与使用 |
| --- | --- |
| `find_texture(key)` | 可空 SDL_Texture 指针，用于绘制命令 |
| `find_atlas(key)` | 可空 const Atlas 指针，包含帧信息 |
| `has_font(key)` / `find_font(key)` | 检查或借用已注册字号字体，注意 key 带字号 |
| `find_sound(key)` / `find_music(key)` | 可空 MIX_Audio 指针；普通播放使用 AudioService |

以上均为借用。加载失败或资源清理后不可使用旧指针；场景切换本身不意味着资源一定清空，但重新开始内容加载会先清除旧项目内容。不要在依赖旧资源的对象仍使用它们时启动重载，也不要假设失败会保留上一套资源。

## 内容入口与预加载

项目根必须包含普通文件 `assets/.elysia_root`，`content_registry.json` 不能替代该标记。默认内容入口为 `assets/content_registry.json`。Bootstrap 在程序启动时仅解析一次，并将解析后的只读 `ContentRegistry` 快照交给 engine Application 持有；Scene 通过 `SceneRuntimeContext` 借用该快照，内容加载阶段不会再次读取此文件。根对象必须且只能包含 `bootstrap` 与 `manifests`；解析前会拒绝重复 JSON 属性，未知字段或目标文件不存在都会返回结构化启动失败。

```json
{
  "bootstrap": {
    "app_config": "configs/global/app_config.json",
    "preload_manifest": "configs/manifests/preload_manifest.json"
  },
  "manifests": {
    "required": {
      "configs": "configs/manifests/config_manifest.json",
      "fonts": "configs/manifests/fonts_manifest.json",
      "audio": "configs/manifests/audio_manifest.json",
      "i18n": "configs/manifests/i18n_manifest.json",
      "textures": "configs/manifests/textures_manifest.json",
      "animations": "configs/manifests/animations_manifest.json",
      "effects": "configs/manifests/effects_manifest.json"
    },
    "additional": {}
  }
}
```

### `bootstrap`

| 字段 | 规则 |
| --- | --- |
| `app_config` | 必填 string；按 `assets/` 解析；Bootstrap 读取固定 AppConfig schema。 |
| `preload_manifest` | 必填 string；按 `assets/` 解析；用于启动纹理预加载。 |

`bootstrap` 不接受其他字段。通用 gameplay 配置不在启动阶段读取。

### `manifests.required`

`required` 必须是对象，且下列七项全部必填：

| 字段 | 目标 |
| --- | --- |
| `configs` | 通用 gameplay 配置 manifest；由内容加载阶段构建快照。 |
| `fonts` | 核心字体 manifest。 |
| `audio` | 核心 Sound/Music manifest。 |
| `i18n` | 国际化 manifest。 |
| `textures` | 核心纹理 manifest。 |
| `animations` | 核心 Animation manifest。 |
| `effects` | 核心 EffectDefinition manifest。 |

每个值必须是 string，按 `assets/` 解析后必须为普通文件。`ContentManifestPipeline` 读取这些声明；`GameContentLoader` 只在 Atlas、纹理、字体、音频、Animation 与 EffectDefinition 全部成功注册后发布 `configs` 生成的 `ConfigSnapshot`。

### `manifests.additional`

`additional` 可省略；存在时必须是 “module 名 → module manifest 路径” 的对象。module 名不决定 loader 类型，所有 module 都使用同一实体资源包 schema，并按名称稳定排序。详见本页“实体内容资源包”。

### `preload_manifest.json`

启动预加载读取显式的 `textures` 条目；独立示例不需要项目 Logo，因此使用空数组：

```json
{
  "textures": []
}
```

需要项目启动纹理时，每项都必须包含稳定资源 key 和相对文件名；路径基于 `assets/preload/`。manifest 中列出的每个纹理都是必需资源；文件缺失、无法解码或无法创建 SDL texture 都会使 phase2 失败，且不会发布部分缓存。

例如，将项目图片放在 `assets/preload/logo.png`，预加载清单写为：

```json
{
  "textures": [{ "key": "project.logo", "file": "logo.png" }]
}
```

在启动加载 Payload 的 project_logo 中使用 `project.logo`。它只属于启动预加载缓存，不自动变成 `ELYSIA_RESOURCES` 中的项目纹理。


## 核心资源清单

所有核心显式资源 key 都按点 `.` 拆分，每个 component 必须匹配 `[A-Za-z0-9_]+`。例如 `ui.moon` 合法，`ui..moon`、`.ui`、`ui-moon` 和 `界面.moon` 非法。

### 字体

```json
{
  "fonts": [
    { "key": "ui.latin", "file": "fusion-pixel.ttf" }
  ]
}
```

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `fonts` | array<object> | 必填且非空 |
| `fonts[].key` | string | 必填、非空、合法点分 key |
| `fonts[].file` | string | 必填、非空，基于 `assets/fonts/` |

字体 manifest 只描述项目字体族文件，不保存字号。字号由 Application 字体设置统一解析；项目未提供覆盖值时使用引擎默认 Typography Profile 和 20pt 浮动数字。每个字体条目根据最终项目字号集合生成 Font 请求，key 为 `<font key>.<size>`，例如 `ui.latin.30`。若 Application 未选择任何项目字体来源，则不会生成项目字体请求。派生的数字尺寸也通过统一 key builder 校验。字体文件的读取与字体创建在资源提交阶段完成。

字体条目在数组中的 JSON pointer 会进入 `ResourceOrigin`。相同派生 Font key 即使来自不同字体条目，也会在请求计划的 Font registry 查重阶段报告 first/second 两个来源。

### 音频

```json
{
  "sounds": {
    "system.confirm": { "path": "system/confirm.wav" }
  },
  "music": {
    "scene.main": { "path": "scene/main.ogg" }
  }
}
```

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `sounds` | object | 必填；属性名成为 Sound key |
| `music` | object | 必填；属性名成为 Music key |
| 条目 `path` | string | 必填；基于 `assets/audio/` |

两个对象都可为空。每个属性名必须是合法点分 key，条目对象只接受 `path`。manifest 会拒绝重复 JSON 对象属性；文件读取和解码错误由后续音频加载阶段报告。

Sound 与 Music 是不同 registry，因此二者使用相同字符串 key 合法；同一 registry 内与 module Audio 冲突则失败，并报告两个完整来源。

### 纹理

```json
{
  "textures": {
    "ui.moon": { "path": "ui/moon.png" }
  }
}
```

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `textures` | object | 必填 |
| 属性名 | string | 合法点分 key，直接成为 Texture key |
| 条目 `path` | string | 必填；基于 `assets/textures/` |

条目对象只接受 `path`。请求生成要求解析结果是普通文件，目录不能作为核心 Texture 条目。manifest 会拒绝重复 JSON 对象属性。

核心 Texture 与所有 module Texture 进入同一个 Texture registry 查重；冲突错误包含两边的配置路径、JSON pointer、core/module、capability、entity 和逻辑名。

### 国际化 manifest

```json
{
  "default_language": "en",
  "languages": ["en", "ja", "ko", "zh-Hans", "zh-Hant"],
  "file": ["base.json"]
}
```

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `default_language` | string | 必填、非空 |
| `languages` | array<string> | 必填且非空 |
| `file` | array<string> | 必填且非空，每项非空 |

语言文件按 `assets/i18n/<language>/<file>` 精确查找。`language` 使用标准 BCP 47 locale（例如 `zh-Hans`），不接受下划线 alias。默认语言不在 `languages` 时，运行时会把它追加为受支持语言。

每个语言目录依次加载 `file` 中的 JSON 并合并翻译表。文件不存在、JSON 无效或翻译数据结构不受支持都会使该语言加载失败。i18n 不进入 Atlas/Animation/Effect/Texture/Font/Sound/Music 的资源 key registry。

### Registry 查重摘要

| 核心配置 | registry | 与 module 合并查重 |
| --- | --- | --- |
| 字体 + 固定尺寸 | Font | 是 |
| `sounds` | Sound | 是 |
| `music` | Music | 是 |
| 纹理 | Texture | 是 |
| 动画 Atlas | Atlas | 是 |
| 动画 | Animation | 是 |
| EffectDefinition | Effect | 是 |

同一字符串跨 registry 合法，例如 Atlas 与 Animation 通常故意共用动画 key。只有同一个 registry 内重复才失败。

## 实体内容资源包

`manifests.additional` 的每项都是一个任意命名的实体资源包。所有包由同一个 loader 处理；`characters`、`character_effects` 和 `enemies` 只是当前配置的名称，不是代码白名单。

### Module manifest

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

### Capability

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

这两类 layout 文件的根对象直接把逻辑名映射为路径字符串。例如 Texture layout 可写 `{"portrait":"portrait.png","icons":"icons"}`，Audio layout 可写 `{"hit":"hit.wav"}`；路径分别相对于实体化后的 texture_root 与 audio_root。不是核心清单的 `{ "path": ... }` 条目结构。动画 layout 的结构见[动画配置](../systems/animation-and-effects.md)。

### `{id}` 与路径解析

`texture_root` 与 `audio_root` 中出现 `{id}` 时，会替换全部标记；未出现时自动在末尾追加实体 id。只允许此 token，实体化后的根目录必须存在。

所有 `config_template` 必须显式包含 `{id}`，替换后必须是存在的普通配置文件。Animation 的 `layouts` 与 Texture/Audio 的 `layout` 都相对于 `assets/` 解析；layout 内的资源路径则相对于实体化后的对应根目录解析。

### Entity manifest

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

### 资源 key

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

### 常见失败

- entity 缺少 id、id 重复、含未知字段或 component 非法；
- 模板缺少 `{id}`、使用未知 token，或解析后的路径不存在；
- `effects` 没有同 module 的 `animations`；
- entity 的 `animation_layout` 不在 capability 的 `layouts` 中；
- Texture/Audio layout 或目录纹理 stem 非法；
- 核心和 module 在同一资源 registry 生成重复 key。错误会列出 first/second 两个完整 `ResourceOrigin`。

## 加载失败与生命周期

项目 preload 条目全部必需；配置错误、文件缺失、解码或注册失败应中止进入依赖该内容的场景。启动 Logo 查询可选与 preload 文件是否必需是不同规则，见[启动加载](../builtin-scenes/startup-loading.md)。preload 纹理不通过 ResourceService 查询，其借用期在 StartupLoadingScene 退出时结束。

正常加载入口由 StartupLoadingScene 驱动，不在普通对象逐帧更新里反复加载。项目资源加载失败会清理已部分提交内容；成功后 loader 的普通 reset 只清理临时状态，新一轮 start 才开启重新加载。

内建资源和项目资源分开管理，项目清理不会清除引擎内建资源。不要用项目 key 覆盖内建保留名称，不自行释放服务返回的字体、音频、纹理或图集。

常见错误是把文件路径当作资源 key、遗漏字号后缀、把 Sound key 当 Music key、资源未加载就查询，以及在重载后继续使用旧指针。相同字符串在不同资源类型中可以共存，同一类型重复注册会失败。

## 相关指南

- [动画与特效配置及播放](../systems/animation-and-effects.md)、[配置读取](configuration.md)、[本地化](../systems/localization.md)
- [资源服务接口](../../../../engine/resources/resource_service.h)、[可选架构参考](../../architecture/subsystems/resources/README.md)
- [返回使用指南](../README.md)
