# Elysia Engine 使用指南

本指南面向使用 Elysia Engine 开发游戏的开发者，提供从环境配置、构建运行到游戏内容开发的文档入口

## 开始使用

首先阅读[构建、运行与测试](build-and-run.md)开始，了解工具链要求、构建命令、运行目录、测试方式与常见问题。

## 开发前必读

在开始开发游戏内容前，请阅读[核心概念](core-concepts.md)这份文档介绍场景、游戏对象与 UI 的职责，以及对象生命周期、更新和销毁时机。


## 从零构建游戏

如果需要从头创建一个基于 Elysia Engine 的游戏，请阅读游戏初始化与集成指南，了解 IGameModel 的职责、游戏入口的组织方式，以及如何注册和初始化游戏内容。

如果只是为已有游戏添加角色、武器、UI 或其他功能，可以跳过本节，直接查阅对应的开发任务文档。


## 按开发任务查阅

| 开发任务 | 文档入口 |
| --- | --- |
| 注册内容、配置预加载与资源清单 | [资源加载与配置格式](../architecture/subsystems/resources/README.md) |
| 配置动作映射、控制器和场景输入 | [输入系统](../architecture/subsystems/input/README.md) |
| 创建 UI 窗口、组织布局与组合控件 | [UI 使用指南](../architecture/subsystems/ui/usage-guide.md) |
| 查询 UI 控件的公开接口 | [UI API 参考](../architecture/subsystems/ui/README.md) |
| 使用刚体、碰撞事件、空间查询与 TileMap 碰撞 | [物理功能与接口总览](../architecture/subsystems/physics/10-physics-features-and-api-guide.md) |
| 配置动画资源、图集与特效 | [动画与特效](../architecture/subsystems/resources/animation-and-effects.md) |
| 播放音乐与音效，控制音量和播放调度 | [音频服务](../architecture/subsystems/audio.md) |
| 使用相机跟随、震屏与坐标转换 | [相机工作流](../architecture/subsystems/camera.md) |
| 管理启动配置、用户设置与游戏配置 | [运行时配置](../architecture/subsystems/runtime-config.md) |
| 读写存档、处理恢复与错误 | [存档服务](../architecture/subsystems/save.md) |


## 常用宏与服务入口

Elysia Engine 提供了一组便利宏，用于访问常用的引擎服务和功能。

以下列出常用宏及其对应的功能文档和源文件。具体接口和使用方式请参考对应文档或头文件。

| 宏名称 | 用途 | 介绍文档 | 源文件 |
| --- | --- | --- | --- |
| `ELYSIA_ANIMATIONS` | 动画服务 | [动画与特效](../architecture/subsystems/resources/animation-and-effects.md) | [animation_service.h](../../../engine/animation/animation_service.h) |
| `ELYSIA_AUDIO` | 音频服务 | [音频服务](../architecture/subsystems/audio.md) | [audio_service.h](../../../engine/audio/audio_service.h) |
| `ELYSIA_CAMERA` | 相机管理 | [相机工作流](../architecture/subsystems/camera.md) | [camera_manager.h](../../../engine/camera/camera_manager.h) |
| `ELYSIA_USER_CONFIG` | 用户配置管理 | [运行时配置](../architecture/subsystems/runtime-config.md) | [user_config_service.h](../../../engine/config/user_config_service.h) |
| `ELYSIA_TIME` | 时间相关功能 | — | [time.h](../../../engine/core/time.h) |
| `ELYSIA_EFFECTS` | 特效服务 | [动画与特效](../architecture/subsystems/resources/animation-and-effects.md) | [effect_service.h](../../../engine/effects/effect_service.h) |
| `ELYSIA_LOCALIZATION` | 本地化服务 | — | [localization_service.h](../../../engine/localization/localization_service.h) |
| `ELYSIA_OBJECT_QUERY` | 游戏对象查询 | — | [game_object_query_service.h](../../../engine/object_query/game_object_query_service.h) |
| `ELYSIA_RESOURCES` | 资源管理 | [资源加载与配置格式](../architecture/subsystems/resources/README.md) | [resource_service.h](../../../engine/resources/resource_service.h) |
| `ELYSIA_SAVE` | 存档管理 | [存档服务](../architecture/subsystems/save.md) | [save_service.h](../../../engine/save/save_service.h) |
| `ELYSIA_DEBUG_DRAW` | 调试绘制 | — | [debug_draw.h](../../../engine/tools/debug_draw.h) |

### 日志宏

引擎提供不同级别的日志宏，用于记录运行信息、调试信息、警告与错误。

| 宏名称 | 日志级别 | 用途 |
| --- | --- | --- |
| `ELYSIA_LOG_DEBUG` | Debug | 调试信息 |
| `ELYSIA_LOG_INFO` | Info | 普通运行信息 |
| `ELYSIA_LOG_WARN` | Warn | 警告信息 |
| `ELYSIA_LOG_ERROR` | Error | 错误信息 |

基本用法：

```cpp
ELYSIA_LOG_INFO("Gameplay", "Game initialized.");
ELYSIA_LOG_ERROR("Resources", "Failed to load texture.");
```
## 错误处理与恢复

Elysia Engine 提供内置错误场景，用于处理无法在当前场景中恢复的错误。

当游戏遇到无法继续正常运行的错误时，可以通过引擎提供的接口切换至错误场景，向用户展示错误信息，而不是直接终止程序。

具体使用方式请参阅[错误处理指南](...).


## 其他通用工具

除常用服务外，Elysia Engine 还提供了一些可复用的开发工具。

| 工具 | 用途 | 源文件 |
| --- | --- | --- |
| `RandomGenerator` | 随机数生成 | [random_generator.h](../../../engine/tools/random_generator.h) |
| `Timer` | 计时功能 | [timer.h](../../../engine/tools/timer.h) |


## 引擎内置通用场景


## 进一步阅读

- [架构与模块设计](../architecture/README.md)：了解引擎内部的职责划分与运行机制。
- [开发与贡献指南](../contributing/README.md)：了解代码规范、测试和文档维护要求。
- [返回项目介绍](../../../README.zh-CN.md)：查看项目定位、功能概览与仓库组成。
