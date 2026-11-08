# 项目启动全流程

本图按当前示例 GameModule 的 initial_route 展示从 main 到主菜单的流程。引擎允许项目指定其他初始路由和成功目标，MainMenu 不是引擎固定入口。

<a id="startup-flow"></a>
## 初始化与运行阶段

```mermaid
flowchart TB
  Entry["main<br/>创建 GameModule"]
  InitFailure["初始化失败<br/>记录诊断并显示错误对话框"]
  Clean["加载失败：停止并回收线程<br/>清理项目内容"]
  Failure["当前进入 ApplicationFailureScene<br/>可配置自定义 failure_route"]
  Shutdown["Application.shutdown<br/>输入 / 场景 / 服务 / 资源<br/>渲染器 / 窗口 / TTF / MIX / SDL / 日志"]
  Exit["main 返回 EXIT_SUCCESS 或 EXIT_FAILURE"]
  subgraph InitPhase["Application.initialize · 同步初始化"]
  Descriptor["日志、终止管理、GameModule.descriptor<br/>校验尺寸、字体设置与 UI 主题"]
  Bootstrap["Bootstrapper.parse_runtime_settings<br/>路径与目录 → 注册表 → 应用与用户配置"]
  Save["文件日志 / SaveService"]
  Platform["initialize_runtime<br/>SDL → MIX → TTF → AudioService<br/>窗口、GPU 渲染器与显示设置"]
  Dev["可选开发覆盖层"]
  Builtins["BuiltinResources → BuiltinMusicPlayer<br/>LocalizationManager → FontResolver"]
  Preload["设置回调 → 输入系统 → 启动纹理预加载"]
  Compose["SceneRuntimeContext / SceneManager<br/>注册引擎、Realm 和游戏场景"]
  Enter["SceneManager.start 初始路由<br/>StartupLoadingScene.on_enter<br/>建立 UI，启动 Logo 和 GameContentLoader.start"]
  end
  subgraph RunPhase["Application.run · 主循环"]
  Loop["事件 / 输入 → 场景和音频更新<br/>渲染 / 呈现 → 帧率控制"]
  Logo["Logo 展示<br/>引擎 → 可选项目 Logo"]
  Load["GameContentLoader.update<br/>计划 → 资源提交 → 动画与特效注册<br/>发布 ConfigSnapshot"]
  Fonts["激活项目字体"]
  Gate["StartupLoadingCompletion<br/>加载完成，按配置等待 Logo"]
  Confirm["当前需要用户确认<br/>wait_for_confirmation = true"]
  Leave["StartupLoadingScene.on_exit<br/>重置加载器、销毁 UI、释放 preload<br/>保留成功加载的项目内容"]
  Menu["MainMenuScene<br/>启动完成，继续同一主循环"]
  end
  Entry -->|initialize| Descriptor
  Descriptor -->|继续初始化| Bootstrap
  Bootstrap -->|提供路径和运行设置| Save
  Save -->|继续| Platform
  Platform -->|继续| Dev
  Dev -->|继续| Builtins
  Builtins -->|继续| Preload
  Preload -->|基础依赖就绪| Compose
  Compose -->|start initial_route| Enter
  Enter -->|initialize 返回成功，main 调用 run| Loop
  Loop -->|逐帧推进展示| Logo
  Loop -->|逐帧推进，工作线程并行准备| Load
  Load -->|加载成功| Fonts
  Fonts -->|激活成功| Gate
  Logo -.->|启用等待时参与；当前 false| Gate
  Gate -->|当前确认策略| Confirm
  Gate -.->|关闭确认时直接切换| Leave
  Confirm -->|用户确认，切换 success_route| Leave
  Leave -->|目标场景进入| Menu
  Bootstrap -.->|启动配置失败；其他初始化失败同样终止| InitFailure
  InitFailure -->|终止启动| Shutdown
  Load -->|加载失败| Clean
  Clean -->|请求失败路由| Failure
  Fonts -->|激活失败| Failure
  Enter -.->|启动 UI 创建失败| Failure
  Loop -.->|不可恢复异常 / 渲染错误| Shutdown
  Failure -->|用户选择退出| Shutdown
  Menu -->|退出应用| Shutdown
  Shutdown -->|依据运行与清理结果| Exit
```

源码：[main.cpp](../../../main.cpp) · [application.cpp](../../../engine/application/application.cpp) · [application_scene_composition.cpp](../../../engine/application/composition/application_scene_composition.cpp) · [bootstrapper.cpp](../../../engine/bootstrap/bootstrapper.cpp) · [startup_loading_scene.cpp](../../../engine/builtin/scenes/startup_loading_scene.cpp)

## 当前示例与引擎选项

当前示例进入 StartupLoading，成功后进入 MainMenu；wait_for_logo_sequence=false、wait_for_confirmation=true，且未配置项目 Logo 或自定义失败路由。Payload 的等待 Logo 默认值为 true；关闭等待不会停止 Logo 播放，关闭确认则可直接切换。

源码：[example_game_module.cpp](../../../game/application/example_game_module.cpp) · [startup_loading_scene_payload.h](../../../engine/builtin/scenes/startup_loading_scene_payload.h) · [startup_loading_flow.h](../../../engine/builtin/scenes/startup_loading_flow.h)

## 时序与失败边界

- initialize 内进入初始场景；on_enter 同步准备清单和计划并启动工作线程。run 开始后才持续更新加载进度和展示，不必等待所有项目内容才进入主循环。
- 工作线程只准备 CPU 数据，GPU 资源创建和提交位于主线程；完整链路见[资源服务图](subsystems/resources/README.md#resource-service-architecture)。
- 启动配置、平台或必需预加载失败会终止初始化。图中的普通初始化失败有日志和对话框路径；不可恢复渲染错误由终止边界处理。
- 内容失败先清理加载线程与项目内容，再请求 failure_route 或内建错误场景；字体激活及启动展示失败进入失败路由。一般场景生命周期异常由 SceneManager 尝试错误场景恢复，无法恢复才终止。
- 退出启动场景释放 preload 缓存，成功加载的项目内容继续保留。应用关闭先结束使用者与播放，再释放资源，最后销毁平台设施；清理失败会影响退出结果。

图中的实线表示主流程，虚线表示条件或异常分支。[启动配置指南](../user-guide/builtin-scenes/startup-loading.md) · [服务总览](services.md) · [场景生命周期](subsystems/scene-lifecycle.md)。
