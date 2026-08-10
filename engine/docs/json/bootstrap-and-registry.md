# 启动注册与预加载

默认入口为 `assets/content_registry.json`。Bootstrap 在程序启动时仅解析一次，并将解析后的只读 `ContentRegistry` 快照交给 engine Application 持有；Scene 通过 `SceneRuntimeContext` 借用该快照，内容加载阶段不会再次读取此文件。根对象必须且只能包含 `bootstrap` 与 `manifests`；解析前会拒绝重复 JSON 属性，未知字段或目标文件不存在都会失败。

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

## `bootstrap`

| 字段 | 规则 |
| --- | --- |
| `app_config` | 必填 string；按 `assets/` 解析；Bootstrap 读取固定 AppConfig schema。 |
| `preload_manifest` | 必填 string；按 `assets/` 解析；用于启动纹理预加载。 |

`bootstrap` 不接受其他字段。通用 gameplay 配置不在启动阶段读取。

## `manifests.required`

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

## `manifests.additional`

`additional` 可省略；存在时必须是 “module 名 → module manifest 路径” 的对象。module 名不决定 loader 类型，所有 module 都使用同一实体资源包 schema，并按名称稳定排序。详见[实体内容 module](entity-content.md)。

## `preload_manifest.json`

启动预加载读取显式的 `textures` 条目，每项都包含稳定资源 key 和相对文件名：

```json
{
  "textures": [
    {
      "key": "moonline.brand.logo",
      "file": "Akil_icon_1024.png"
    }
  ]
}
```

项目纹理路径基于 `assets/preload/`，运行时使用条目的 `key`。manifest 中列出的每个纹理都是必需资源；文件缺失、无法解码或无法创建 SDL texture 都会使 phase2 失败，且不会发布部分缓存。

`assets/engine` 由 Application 持有的 `BuiltinAssetCache` 在 renderer 建立后事务式加载：五张 Engine 纹理、五套七档字号字体及五语 `engine.json` 都常驻到 Application shutdown，且不注册到项目 `ResourceManager`。其中 `.elysia_engine_required` 是必需根标记；`OFL.txt` 仅为许可证文件。Elysia Logo 由 `StartupLoadingScene` 直接从 Builtin Asset Cache 取得，因此 `GameContentLoader` 清理或重新加载项目内容不会影响内建启动场景。

`BootstrapTextureCache` 仅持有项目 preload manifest 中的启动纹理。`find_preload_texture()` 返回可空的借用指针且查询本身不记录日志；其有效期截止到 `StartupLoadingScene::on_exit()`，Application shutdown 仍保留幂等兜底释放。
