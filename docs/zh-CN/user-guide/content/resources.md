# 资源加载与使用

本文面向准备游戏素材与内容清单的开发者。先说明资源如何从声明进入加载流程，再介绍各类配置与查询方式；配置示例遵循当前格式，不表示可任意增加字段。

资源由应用和内容加载流程统一加载。游戏代码使用 `ELYSIA_RESOURCES` 查询已加载的资源，不自行初始化或关闭资源管理器。先完成[游戏初始化](../game-initialization.md)，通过[启动加载场景](../builtin-scenes/startup-loading.md)加载内容，成功后再进入使用这些资源的场景。

## 最小使用流程

1. 在下文对应资源清单中声明资源，将文件放在规定的资源根目录。
2. 确保 `assets/content_registry.json` 引用这些资源清单。修改注册表、启动配置或预加载声明后重启应用；注册表在启动时解析，内容加载不会重新读取它。
3. 加载成功后按资源键查询并处理空结果。纹理交给[游戏对象](../scene/game-objects.md)绘制，音频交给[音频服务](../systems/audio.md)播放。修改正式内容清单也不会自动热重载；开发时重启应用是最直接的验证方式。

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
| `find_texture(key)` | 可空 `SDL_Texture` 指针，用于绘制命令 |
| `find_atlas(key)` | 可空 const `Atlas` 指针，包含帧信息 |
| `has_font(key)` / `find_font(key)` | 检查或借用已注册字号字体，注意资源键带字号 |
| `find_sound(key)` / `find_music(key)` | 可空 MIX_Audio 指针；普通播放使用 `AudioService` |

以上均为借用。加载失败或资源清理后不可使用旧指针；场景切换本身不意味着资源一定清空，但重新开始内容加载会先清除旧项目内容。不要在依赖旧资源的对象仍使用它们时启动重载，也不要假设失败会保留上一套资源。

## 内容入口与预加载

项目根必须包含普通文件 `assets/.elysia_root`，`content_registry.json` 不能替代该标记。默认内容入口为 `assets/content_registry.json`。Bootstrap 在程序启动时仅解析一次，并将解析后的只读 `ContentRegistry` 快照交给引擎 `Application` 持有；场景通过 `SceneRuntimeContext` 借用该快照，内容加载阶段不会再次读取此文件。

根对象必须且只能包含 `bootstrap` 与 `manifests`；解析前会拒绝重复 JSON 属性，未知字段或目标文件不存在都会返回结构化启动失败。

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
| `app_config` | 必填 string；按 `assets/` 解析；Bootstrap 读取固定 `AppConfig` 结构约定。 |
| `preload_manifest` | 必填 string；按 `assets/` 解析；用于启动纹理预加载。 |

`bootstrap` 不接受其他字段。通用游戏玩法配置不在启动阶段读取。

### `manifests.required`

`required` 必须是对象，且下列七项全部必填：

| 字段 | 目标 |
| --- | --- |
| `configs` | 通用游戏玩法配置资源清单；由内容加载阶段构建快照。 |
| `fonts` | 核心字体资源清单。 |
| `audio` | 核心 Sound/Music 资源清单。 |
| `i18n` | 国际化资源清单。 |
| `textures` | 核心纹理资源清单。 |
| `animations` | 核心 `Animation` 资源清单。 |
| `effects` | 核心 EffectDefinition 资源清单。 |

每个值必须是 string，按 `assets/` 解析后必须为普通文件。国际化清单由应用初始化本地化时读取，其余正式内容清单进入内容加载流程。加载器只在图集、纹理、字体、音频、动画与特效全部成功注册后发布游戏配置快照；读取 `ELYSIA_CONFIG` 应等到内容加载完成。

### `manifests.additional`

`additional` 可省略；存在时必须是 “模块名 → 模块资源清单路径” 的对象。模块名不决定加载器类型，所有模块都使用同一实体资源包结构约定，并按名称稳定排序。详见本页“实体内容资源包”。

### `preload_manifest.json`

启动预加载读取显式的 `textures` 条目；独立示例不需要项目 Logo，因此使用空数组：

```json
{
  "textures": []
}
```

需要项目启动纹理时，每项都必须包含稳定资源键和相对文件名；路径基于 `assets/preload/`。资源清单中列出的每个纹理都是必需资源；文件缺失、无法解码或无法创建 SDL texture 都会使第二启动阶段失败，且不会发布部分缓存。

例如，将项目图片放在 `assets/preload/logo.png`，预加载清单写为：

```json
{
  "textures": [{ "key": "project.logo", "file": "logo.png" }]
}
```

在启动加载 Payload 的 project_logo 中使用 `project.logo`。它只属于启动预加载缓存，不自动变成 `ELYSIA_RESOURCES` 中的项目纹理。


## 核心资源清单

所有核心显式资源键都按点 `.` 拆分，每个组成部分必须匹配 `[A-Za-z0-9_]+`。例如 `ui.moon` 合法，`ui..moon`、`.ui`、`ui-moon` 和 `界面.moon` 非法。

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
| `fonts[].key` | string | 必填、非空、合法点分资源键 |
| `fonts[].file` | string | 必填、非空，基于 `assets/fonts/` |

字体资源清单只描述项目字体族文件，不保存字号。字号由 `Application` 字体设置统一解析；项目未提供覆盖值时使用引擎默认 Typography Profile 和 20pt 浮动数字。每个字体条目根据最终项目字号集合生成 Font 请求，资源键为 `<font key>.<size>`，例如 `ui.latin.30`。若 `Application` 未选择任何项目字体来源，则不会生成项目字体请求。派生的数字尺寸也通过统一资源键构造器校验。字体文件的读取与字体创建在资源提交阶段完成。

字体条目在数组中的 JSON 路径指针会进入 `ResourceOrigin`。相同派生 Font 资源键即使来自不同字体条目，也会在请求计划的 Font 注册表查重阶段报告首次与重复声明两个来源。

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
| `sounds` | object | 必填；属性名成为 Sound 资源键 |
| `music` | object | 必填；属性名成为 Music 资源键 |
| 条目 `path` | string | 必填；基于 `assets/audio/` |

两个对象都可为空。每个属性名必须是合法点分资源键，条目对象只接受 `path`。资源清单会拒绝重复 JSON 对象属性；文件读取和解码错误由后续音频加载阶段报告。

Sound 与 Music 是不同注册表，因此二者使用相同字符串资源键合法；同一注册表内与模块 Audio 冲突则失败，并报告两个完整来源。

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
| 属性名 | string | 合法点分资源键，直接成为 Texture 资源键 |
| 条目 `path` | string | 必填；基于 `assets/textures/` |

条目对象只接受 `path`。请求生成要求解析结果是普通文件，目录不能作为核心 Texture 条目。资源清单会拒绝重复 JSON 对象属性。

核心 Texture 与所有模块 Texture 进入同一个 Texture 注册表查重；冲突错误包含两边的配置路径、JSON 路径指针、core/module、能力声明、实体和逻辑名。

### 国际化资源清单

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

每个语言目录依次加载 `file` 中的 JSON 并合并翻译表。文件不存在、JSON 无效或翻译数据结构不受支持都会使该语言加载失败。i18n 不进入 Atlas/Animation/Effect/Texture/Font/Sound/Music 的资源键注册表。

### 资源键的查重范围

| 核心配置 | 注册表 | 与模块合并查重 |
| --- | --- | --- |
| 字体 + 固定尺寸 | Font | 是 |
| `sounds` | Sound | 是 |
| `music` | Music | 是 |
| 纹理 | Texture | 是 |
| 动画 `Atlas` | `Atlas` | 是 |
| 动画 | `Animation` | 是 |
| EffectDefinition | `Effect` | 是 |

同一字符串跨注册表合法，例如 `Atlas` 与 `Animation` 通常故意共用动画资源键。只有同一个注册表内重复才失败。

## 实体内容资源包

`manifests.additional` 的每项都是一个任意命名的实体资源包。所有包由同一个加载器处理；`characters`、`character_effects` 和 `enemies` 只是当前配置的名称，不是代码白名单。

### 模块资源清单

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

根对象只能包含 `entities`、`key_namespace`、`capabilities`。三项都必填；`key_namespace` 可以是空字符串，`capabilities` 可以为空对象。允许的能力声明固定为可选的 `animations`、`effects`、`textures`、`audio`；未知字段或能力声明会失败。`effects` 存在时，同一模块必须同时提供 `animations`。

### 能力声明

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

Texture 和 Audio 能力声明都要求根目录和布局。Texture 布局可指向文件或非递归目录；目录中的直接文件按路径排序，不含扩展名的文件名参与资源键。Audio 布局的目标必须是普通文件，当前只生成 Sound。

这两类布局文件的根对象直接把逻辑名映射为路径字符串。例如 Texture 布局可写 `{"portrait":"portrait.png","icons":"icons"}`，Audio 布局可写 `{"hit":"hit.wav"}`；路径分别相对于实体化后的 texture_root 与 audio_root。不是核心清单的 `{ "path": ... }` 条目结构。动画布局的结构见[动画配置](../systems/animation-and-effects.md)。

### `{id}` 与路径解析

`texture_root` 与 `audio_root` 中出现 `{id}` 时，会替换全部标记；未出现时自动在末尾追加实体 id。只允许此标记，实体化后的根目录必须存在。

所有 `config_template` 必须显式包含 `{id}`，替换后必须是存在的普通配置文件。`Animation` 的 `layouts` 与 Texture/Audio 的 `layout` 都相对于 `assets/` 解析；布局内的资源路径则相对于实体化后的对应根目录解析。

### 实体资源清单

```json
{
  "entities": [
    {
      "id": "RyougiShiki",
      "enabled": true,
      "animation_layout": "fighter"
    }
  ]
}
```

| 字段 | 规则 |
| --- | --- |
| `id` | 必填、唯一、合法资源键组成部分；用于目录模板和运行时资源键的首段。当前实体采用 PascalCase 约定。 |
| `enabled` | 可选，默认 `true`；为 `false` 时跳过该实体。 |
| `animation_layout` | 使用 `Animation` 能力声明时必填，且必须匹配该能力声明的 `layouts` 键；若提供也必须是合法组成部分。 |

本例与上面的模块声明配套，选择 `fighter` 布局；相应目录、配置和图片仍须由项目提供。实体条目只能包含上述字段。`id` 的语法是 `[A-Za-z0-9_]+`，PascalCase 是仓库命名约定。显示名属于游戏自己的本地化数据，不是这里可增加的实体字段。

### 资源键

所有能力声明使用同一个资源键构造器：

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

每个组成部分使用 `[A-Za-z0-9_]+`；点仅用于连接组成部分。空组成部分、横线、空格、非 ASCII 字符和连续/首尾点都非法。分段资源键永不补位；补两位只属于文件系统的目录与文件名前缀。

### 常见失败

- 实体缺少 id、id 重复、含未知字段或组成部分非法；
- 模板缺少 `{id}`、使用未知标记，或解析后的路径不存在；
- `effects` 没有同模块的 `animations`；
- 实体的 `animation_layout` 不在能力声明的 `layouts` 中；
- Texture/Audio 布局或目录纹理不含扩展名的文件名非法；
- 核心和模块在同一资源注册表生成重复资源键。错误会列出首次与重复声明两个完整 `ResourceOrigin`。

## 加载失败与生命周期

项目 preload 条目全部必需；配置错误、文件缺失、解码或注册失败应中止进入依赖该内容的场景。启动 Logo 查询可选与 preload 文件是否必需是不同规则，见[启动加载](../builtin-scenes/startup-loading.md)。preload 纹理不通过 `ResourceService` 查询，其借用期在 `StartupLoadingScene` 退出时结束。

正常加载入口由 `StartupLoadingScene` 驱动，不在普通对象逐帧更新里反复加载。项目资源加载失败会清理已部分提交内容；成功后加载器的普通 reset 只清理临时状态，新一轮 start 才开启重新加载。

内建资源和项目资源分开管理，项目清理不会清除引擎内建资源。不要用项目资源键覆盖内建保留名称，不自行释放服务返回的字体、音频、纹理或图集。

常见错误是把文件路径当作资源键、遗漏字号后缀、把 Sound 资源键当 Music 资源键、资源未加载就查询，以及在重载后继续使用旧指针。相同字符串在不同资源类型中可以共存，同一类型重复注册会失败。

## 相关指南

- [动画与特效配置及播放](../systems/animation-and-effects.md)、[配置读取](configuration.md)、[本地化](../systems/localization.md)
- [资源服务接口](../../../../engine/resources/resource_service.h)、[可选架构参考](../../architecture/subsystems/resources/README.md)
- [返回使用指南](../README.md)
