# Elysia Engine 架构总览

## 定位与边界

Elysia Engine 是一个 C++23 二维游戏运行时。核心代码位于 `engine/`，示例项目位于 `game/`。两者通过 `elysia::application::IGameModule` 连接：引擎拥有启动、主循环和共享服务，项目层提供呈现描述、初始路由和场景注册。

```text
main.cpp
  -> example::application::GameModule
  -> elysia::application::Application
       -> bootstrap/config/resources
       -> input/audio/camera/save
       -> SceneManager
            -> built-in scenes
            -> example game scenes
```

引擎代码不得 include `game/` 类型。项目层可以依赖 `engine_lib`，并通过场景、配置、资源和输入 Action 扩展运行时。

## 构建目标

```text
ElysiaEngine executable
  -> game_lib
       -> engine_lib
            -> SDL2 libraries
```

- `engine_lib`：可复用引擎实现。
- `game_lib`：示例 `IGameModule` 和示例场景。
- `ElysiaEngine`：组合两层的示例可执行程序。
- `tests/*`：直接链接 `engine_lib` 的子系统测试目标。

## 启动流程

1. `main.cpp` 创建 `example::application::GameModule`。
2. `Application::initialize()` 读取模块的 `ApplicationDescriptor`。
3. Bootstrap 解析启动配置和内容注册表，初始化 SDL、窗口、渲染器、音频、用户配置、内建资源、本地化和共享服务。
4. `SceneRuntimeContext` 向所有场景提供渲染器、逻辑分辨率、内容注册表和字体等通用能力；built-in 资源不进入通用场景上下文。
5. 项目模块注册场景，`SceneManager` 进入 `initial_route`。
6. `Application::run()` 推进事件、输入、场景更新、音频调度和渲染，直到收到正常退出或发生运行时失败。

初始化任一步骤失败都会阻止主循环启动；程序入口根据 `ApplicationRunResult` 返回成功或失败退出码。

## 场景与项目扩展

引擎内建场景负责启动加载、设置和失败处理等通用流程。`game/` 中的场景只用于展示项目如何注册自己的路由、菜单和 Sandbox。新的项目层应自行定义场景 key 和业务 Action，不应把它们加入引擎的标准集合。

## 子系统关系

- Bootstrap、配置和资源加载负责在进入实际内容前建立可用的运行时状态。
- Raw Input 由 `InputSystem` 采集；Action Mapping 和 Gameplay Input 在需要游戏语义的场景中继续转换。
- UI 使用 Raw/UI 语义输入，不依赖项目的玩法 Action。
- 场景通过运行时上下文和受控的共享服务访问渲染、音频、相机、存档、资源与配置。
- Physics 当前提供数据契约、策略接口和服务门面；完整目标架构见物理模块实现指南。

各模块的详细约定见[子系统索引](../subsystems/README.md)。
