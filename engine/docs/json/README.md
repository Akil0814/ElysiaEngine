# 资源加载 JSON 文档

本目录记录 Moonline 当前资源加载管线实际接受的 JSON。它面向新增公共资源、实体资源包、动画和特效的维护者；未实现或已删除的兼容格式不在文档中保留。

## 文档索引

- [启动注册与预加载](bootstrap-and-registry.md)：`content_registry.json`、required/additional 入口与 `preload_manifest.json`。
- [核心资源 manifest](core-resource-manifests.md)：字体、音频、纹理和国际化。
- [实体内容 module](entity-content.md)：任意命名的 additional module、entity manifest、纹理和音频 layout。
- [动画与特效](animation-and-effects.md)：核心/实体 Animation、Atlas 来源、分段编号和 EffectDefinition。
- [运行时配置模块](../runtime-config.md)：AppConfig、UserConfig 与内容加载完成后发布快照的通用 ConfigService。

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

`additional` 不再按 module 名选择专用 loader。所有 module 都使用同一个 schema，并按 module 名的确定性顺序遍历。当前三个名称只是仓库配置，不是硬编码白名单。

资源请求按以下阶段组装：

1. 核心 Animation/Atlas。
2. 全部 additional module 的 Animation/Atlas。
3. 核心及 module 的 EffectDefinition。
4. 核心及 module 的 Texture。
5. Font、核心 Audio 和 module Audio。

运行时仍先完成 Atlas，再注册全部 Animation，最后注册 EffectDefinition。因此 Effect 只绑定已经生成的 Animation，不负责加载图片或创建 Animation。

## 路径根目录

| 配置内容 | 相对路径根 |
| --- | --- |
| registry、module manifest、layout 中的 `configs/...` | `assets/` |
| 核心字体 manifest 的 `file` | `assets/fonts/` |
| 核心音频 manifest 的 `path` | `assets/audio/` |
| 核心纹理和动画 manifest 的 `path` | `assets/textures/` |
| preload manifest 的纹理路径 | `assets/preload/` |
| i18n 文件 | `assets/i18n/<language>/` |
| module Animation/Texture layout 的相对路径 | 对应实体解析后的 `texture_root` |
| module Audio layout 的相对路径 | 对应实体解析后的 `audio_root` |

`PathManager` 保留绝对路径；以 `assets` 开头的相对路径基于项目根解析，以 `configs` 开头的配置路径基于 `assets/` 解析。

## 资源 key 语法

所有显式 key 以及构成派生 key 的 component 都采用同一规则：

```text
[A-Za-z0-9_]+
```

点 `.` 只用于连接 component。空 component、连续点、首尾点、横线、空格、非 ASCII 字符以及其他分隔符都非法；纯数字 component 合法。规则覆盖 entity id、非空 `key_namespace`、layout 名、动画/特效/纹理/音频逻辑名、目录纹理的 file stem，以及按点拆分后的核心显式 key。实体 id 约定使用 PascalCase。`key_namespace: ""` 是唯一允许为空的 key 输入，构建 key 时会直接跳过。

module 资源统一构造为：

```text
<entity id>[.<key_namespace>].<logical component>[.<segment index>]
```

segment 的资源 key 使用无补位十进制，例如 `.0`、`.1`、`.99`。两位补位只属于文件系统路径和文件名前缀，详见[动画与特效](animation-and-effects.md#分段编号与文件名)。

## 来源与重复 key

每个资源请求都携带 `ResourceOrigin`，用于错误定位：

- 项目相对配置路径；
- JSON pointer 或数组索引；
- 独立的 `core`/`additional` scope，以及 additional module 名（module 即使命名为 `core` 或空字符串也不会与核心来源混淆）；
- capability；
- entity id、逻辑名及 segment/index（适用时）。

请求提交前会把核心与全部 module 合并检查，并按运行时 registry 分别查重：`Atlas`、`Animation`、`Effect`、`Texture`、`Font`、`Sound`、`Music`。同一 registry 中 key 重复会终止组装，错误同时打印 first/second 两个完整来源；不同 registry 使用同一字符串合法。

## 通用约定

- JSON 文件必须可读且语法有效；配置对象中的未知字段是否允许，以对应章节为准。
- 多个关键配置会在解析前拒绝重复 JSON 对象属性，避免解析器静默覆盖。
- 同一 JSON object 中重复书写同名 member 属于 schema 错误，会在资源声明生成前失败；`first`/`second` `ResourceOrigin` 诊断针对能够由 schema 正常表示、但最终落入同一 registry key 的两条独立资源声明。
- 路径存在性可能在 registry、module 加载、请求生成或资源准备阶段检查；任何阶段失败都会终止对应加载并写入日志。
- 本目录不描述 `app_config`、`input_config`、`game_config`、地图、关卡、角色战斗数值、技能或用户设置等业务 JSON 的内部结构。
