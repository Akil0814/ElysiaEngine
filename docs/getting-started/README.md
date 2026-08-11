# 快速开始

Elysia Engine 当前以源码仓库形式提供。最直接的使用方式是构建仓库中的 `ElysiaEngine` 示例程序，然后从 `game/` 观察项目如何接入 `engine_lib`。

## 阅读顺序

1. 按[构建、运行与测试](build-and-run.md)完成一次 Debug 构建。
2. 阅读[架构总览](../architecture/overview.md)，了解引擎层与示例游戏层的边界。
3. 查看 `game/application/example_game_module.cpp`，了解应用描述、初始路由和场景注册。
4. 根据需求进入[子系统索引](../subsystems/README.md)。

## 最小集成模型

项目层实现 `elysia::application::IGameModule`：

- `descriptor()` 返回逻辑分辨率、呈现设置和初始场景路由。
- `register_scenes()` 将项目场景注册到 `SceneManager`。

程序入口把模块交给 `Application::initialize()`，初始化成功后调用 `Application::run()`。仓库中的 `example::application::GameModule` 是这一模式的可运行示例。

## 当前边界

- 引擎以静态库目标 `engine_lib` 提供，没有独立安装和包管理流程。
- `game_lib` 是示例集成层，不是引擎公共 API 的一部分。
- Windows x64 是当前明确验证的平台。
- 配置和资源示例依赖仓库中的 `assets/` 目录；运行可执行文件时需要保留项目目录结构。
