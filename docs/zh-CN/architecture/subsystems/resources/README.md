# 资源加载管线

本页解释资源请求如何组装、提交及交给各服务。声明素材的步骤见[资源指南](../../../user-guide/content/resources.md)，完整 JSON 契约见[资源格式参考](../../../reference/resources/README.md)。

<a id="resource-service-architecture"></a>
## 总加载链路

加载器负责准备与提交，服务只查询已经注册的资源。

```mermaid
flowchart TB
  Registry["ContentRegistry<br/>清单入口快照"]
  Pipeline["ContentManifestPipeline / ResourceRequestAssembler<br/>解析、构建请求、校验与预检查"]
  Loader["GameContentLoader<br/>持有 ResourceLoadPlan，编排加载"]
  Worker["SurfaceLoader / AtlasBuildPreparer<br/>工作线程：CPU 解码与准备"]
  Commit["主线程提交<br/>TextureLoader 与图集帧提交"]
  Game["游戏与其他服务"]
  Service["ResourceService / ELYSIA_RESOURCES<br/>按键查询已加载资源"]
  Manager["ResourceManager<br/>统一写入、查询与清理"]
  Texture["TextureManager<br/>纹理与覆盖掩码"]
  Atlas["AtlasManager<br/>图集与构建状态"]
  Font["FontManager<br/>字号字体"]
  Audio["AudioManager<br/>音效与音乐资源"]
  Cleanup["clear_loaded_content<br/>特效 → 动画 → 配置 → 资源"]
  Registry -->|读取入口| Pipeline
  Pipeline -->|生成加载计划| Loader
  Loader -->|派发准备任务| Worker
  Worker -->|通过结果队列交接| Commit
  Commit -->|store_texture / commit_prepared_atlas_frame| Manager
  Loader -->|启动图集构建，后续加载字体和音频| Manager
  Game -->|find_* / has_*| Service
  Service -->|转发查询，返回借用指针| Manager
  Manager -->|持有| Texture
  Manager -->|持有| Atlas
  Manager -->|持有| Font
  Manager -->|持有| Audio
  Atlas -.->|依赖：保存帧纹理与掩码| Texture
  Loader -->|重新加载或失败时清理| Cleanup
  Cleanup -->|clear：图集先于纹理释放| Manager
```

所有查询指针均为借用。重新加载和失败会清理项目内容；ResourceManager 内按 Atlas、Texture、Font、Audio 顺序清理。BuiltinResources 与 BootstrapTextureCache 独立持有内建和预加载资源，不进入项目资源池。

源码：[resource_service.cpp](../../../../../engine/resources/resource_service.cpp) · [resource_manager.cpp](../../../../../engine/resources/runtime/resource_manager.cpp) · [game_content_loader.cpp](../../../../../engine/loading/game_content_loader.cpp)

注册表的启动配置、预加载与本地化入口由启动和应用初始化处理；图中的项目内容加载使用 required 与 additional 中对应的资源清单。`additional` 不按模块名选择专用加载器；项目新增模块时仍使用同一结构约定，并按模块名的确定性顺序遍历。`ryougi_sample` 的命名空间为空，只声明动画能力；实际配置链路见[资源参考](../../../reference/resources/entity-content.md#当前仓库样例)。

通用加载器按以下阶段组装请求；某类能力未声明时，该阶段没有对应模块请求：

1. 核心 Animation/Atlas。
2. 全部附加内容模块的 Animation/Atlas。
3. 核心及模块的 EffectDefinition。
4. 核心及模块的 Texture。
5. Font、核心 Audio 和模块 Audio。

运行时仍先完成 `Atlas`，再注册全部 `Animation`，最后注册 EffectDefinition。因此 `Effect` 只绑定已经生成的 `Animation`，不负责加载图片或创建 `Animation`。


资源请求的来源、资源键及冲突检查约定见[格式参考](../../../reference/resources/README.md#来源与重复-key)。配置快照的发布边界见[运行时配置](../runtime-config.md)。
