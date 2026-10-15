# 资源加载 JSON 文档

阅读时先区分启动所需的内置资源、项目预加载纹理和正式游戏内容。它们的所有者与释放时机不同，后文按加载顺序给出配置入口和失败边界。

本目录记录 Elysia Engine 当前资源加载管线实际接受的 JSON。它面向新增公共资源、实体资源包、动画和特效的维护者；未实现或已删除的兼容格式不在文档中保留。

## 文档索引

- [启动注册与预加载](bootstrap-and-registry.md)：`content_registry.json`、required/additional 入口与 `preload_manifest.json`。
- [核心资源 manifest](core-resource-manifests.md)：字体、音频、纹理和国际化。
- [实体内容模块](entity-content.md)：任意命名的附加内容模块、实体资源清单、纹理和音频布局。
- [动画与特效](animation-and-effects.md)：核心/实体 `Animation`、`Atlas` 来源、分段编号和 EffectDefinition。
- [运行时配置模块](../runtime-config.md)：`AppConfig`、`UserConfig` 与内容加载完成后发布快照的通用 `ConfigService`。

## 总加载链路

```text
assets/content_registry.json
  ├─ bootstrap.app_config
  ├─ bootstrap.preload_manifest ─────────────> 启动纹理预加载
  ├─ manifests.required
  │   ├─ configs ────────────────────────────> 通用 ConfigSnapshot（内容加载完成后发布）
  │   ├─ animations ─────────────────────────> 核心 Atlas + Animation
  │   ├─ effects ────────────────────────────> 核心 EffectDefinition
  │   ├─ textures/fonts/audio ───────────────> 核心资源
  │   ├─ i18n ───────────────────────────────> LocalizationManager
  └─ manifests.additional
      ├─ characters ─────────────────────────> 本体 Animation/Texture/Audio
      ├─ character_effects ──────────────────> 特效 Animation + EffectDefinition
      ├─ enemies ────────────────────────────> 敌人 Animation
      └─ <任意其他 module 名> ───────────────> 同一种通用 module loader
```

`additional` 不再按模块名选择专用加载器。所有模块都使用同一个结构约定，并按模块名的确定性顺序遍历。当前三个名称只是仓库配置，不是硬编码白名单。

资源请求按以下阶段组装：

1. 核心 Animation/Atlas。
2. 全部附加内容模块的 Animation/Atlas。
3. 核心及模块的 EffectDefinition。
4. 核心及模块的 Texture。
5. Font、核心 Audio 和模块 Audio。

运行时仍先完成 `Atlas`，再注册全部 `Animation`，最后注册 EffectDefinition。因此 `Effect` 只绑定已经生成的 `Animation`，不负责加载图片或创建 `Animation`。

## 路径根目录

| 配置内容 | 相对路径根 |
| --- | --- |
| 注册表、模块资源清单、布局中的 `configs/...` | `assets/` |
| 核心字体资源清单的 `file` | `assets/fonts/` |
| 核心音频资源清单的 `path` | `assets/audio/` |
| 核心纹理和动画资源清单的 `path` | `assets/textures/` |
| preload 资源清单的纹理路径 | `assets/preload/` |
| i18n 文件 | `assets/i18n/<language>/` |
| 模块 Animation/Texture 布局的相对路径 | 对应实体解析后的 `texture_root` |
| 模块 Audio 布局的相对路径 | 对应实体解析后的 `audio_root` |

`PathManager` 保留绝对路径；以 `assets` 开头的相对路径基于项目根解析，以 `configs` 开头的配置路径基于 `assets/` 解析。

## 资源 key 语法

所有显式资源键以及构成派生资源键的组成部分都采用同一规则：

```text
[A-Za-z0-9_]+
```

点 `.` 只用于连接组成部分。空组成部分、连续点、首尾点、横线、空格、非 ASCII 字符以及其他分隔符都非法；纯数字组成部分合法。规则覆盖实体 id、非空 `key_namespace`、布局名、动画/特效/纹理/音频逻辑名、目录纹理的 file 不含扩展名的文件名，以及按点拆分后的核心显式资源键。实体 id 约定使用 PascalCase。`key_namespace: ""` 是唯一允许为空的资源键输入，构建资源键时会直接跳过。

模块资源统一构造为：

```text
<entity id>[.<key_namespace>].<logical component>[.<segment index>]
```

分段的资源键使用无补位十进制，例如 `.0`、`.1`、`.99`。两位补位只属于文件系统路径和文件名前缀，详见[动画与特效](animation-and-effects.md#帧来源与分段)。

## 来源与重复 key

每个资源请求都携带 `ResourceOrigin`，用于错误定位：

- 项目相对配置路径；
- JSON 路径指针或数组索引；
- 独立的 `core`/`additional` 来源范围，以及附加内容模块名（模块即使命名为 `core` 或空字符串也不会与核心来源混淆）；
- 能力声明；
- 实体 id、逻辑名及 segment/index（适用时）。

请求提交前会把核心与全部模块合并检查，并按运行时注册表分别查重：`Atlas`、`Animation`、`Effect`、`Texture`、`Font`、`Sound`、`Music`。同一注册表中资源键重复会终止组装，错误同时打印首次与重复声明两个完整来源；不同注册表使用同一字符串合法。

## 通用约定

- JSON 文件必须可读且语法有效；配置对象中的未知字段是否允许，以对应章节为准。
- 多个关键配置会在解析前拒绝重复 JSON 对象属性，避免解析器静默覆盖。
- 同一 JSON object 中重复书写同名 member 属于结构约定错误，会在资源声明生成前失败；`first`/`second` `ResourceOrigin` 诊断针对能够由结构约定正常表示、但最终落入同一注册表资源键的两条独立资源声明。
- 路径存在性可能在注册表、模块加载、请求生成或资源准备阶段检查；任何阶段失败都会终止对应加载并写入日志。
- 本目录不描述 `app_config`、`input_config`、`game_config`、地图、关卡、角色战斗数值、技能或用户设置等业务 JSON 的内部结构。
