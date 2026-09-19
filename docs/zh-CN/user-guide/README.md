# Elysia Engine 使用指南

本指南面向使用 Elysia Engine 开发游戏的开发者，提供从环境配置、构建运行到游戏内容开发的文档入口。

## 开始使用

从[构建、运行与测试](build-and-run.md)开始，了解工具链要求、构建命令、运行目录、测试方式与常见问题。

## 开发前必读

在开始开发游戏内容前，建议阅读以下文档：

- [核心概念](core_concepts.md)：了解场景、游戏对象与 UI 的职责，以及对象生命周期、更新和销毁时机。

## 从零构建游戏

如果需要从头创建一个基于 Elysia Engine 的游戏，请阅读[游戏初始化与集成指南](game-initialization.md)，了解 `IGameModule` 的职责、游戏入口的组织方式，以及如何注册和初始化游戏内容。

如果只是为已有游戏添加角色、武器、UI 或其他功能，可以跳过本节，直接查阅对应的开发任务文档。

## Gameplay 相关功能

Elysia Engine 提供游戏玩法相关的基础模块，用于组织游戏场景、角色控制及游戏碰撞逻辑。

| 模块 | 用途 | 使用文档 |
| --- | --- | --- |
| Gameplay Collision | 游戏玩法碰撞、碰撞事件与关系处理 | [游戏玩法碰撞](gameplay/collision.md) |
| Gameplay Control | 控制器管理、控制命令与场景控制上下文 | [控制器与控制命令](gameplay/control.md) |
| GameplayScene | 游戏玩法场景 | [GameplayScene 使用指南](gameplay/scene.md) |

## 按开发任务查阅

以下文档按照开发任务分类，可根据需要选择阅读。

| 开发任务 | 文档入口 |
| --- | --- |
| 注册内容、配置预加载与资源清单 | [资源加载与配置格式](../architecture/subsystems/resources/README.md) |
| 配置输入设备、动作映射与场景输入 | [输入系统](../architecture/subsystems/input/README.md) |
| 创建 UI 窗口、组织布局与组合控件 | [UI 使用指南](../architecture/subsystems/ui/usage-guide.md) |
| 查询 UI 控件的公开接口 | [UI API 参考](../architecture/subsystems/ui/README.md) |
| 使用刚体、物理碰撞事件、空间查询与 TileMap 碰撞 | [物理功能与接口总览](../architecture/subsystems/physics/10-physics-features-and-api-guide.md) |
| 配置动画资源、图集与特效 | [动画与特效](../architecture/subsystems/resources/animation-and-effects.md) |
| 播放音乐与音效，控制音量和播放调度 | [音频服务](../architecture/subsystems/audio.md) |
| 使用相机跟随、震屏与坐标转换 | [相机工作流](../architecture/subsystems/camera.md) |
| 管理启动配置、用户设置与游戏配置 | [运行时配置](../architecture/subsystems/runtime-config.md) |
| 读写存档、处理恢复与错误 | [存档服务](../architecture/subsystems/save.md) |

## 常用宏与服务入口

Elysia Engine 为部分常用服务提供便利宏，方便游戏开发者访问相关功能。

以下列出常用宏及其用途、相关文档和接口定义文件。具体使用方式请参考对应文档或头文件。

| 宏名称 | 用途 | 介绍文档 | 源文件 |
| --- | --- | --- | --- |
| `ELYSIA_ANIMATIONS` | 动画服务 | [动画与特效](../architecture/subsystems/resources/animation-and-effects.md) | [animation_service.h](../../../engine/animation/animation_service.h) |
| `ELYSIA_AUDIO` | 音频服务 | [音频服务](../architecture/subsystems/audio.md) | [audio_service.h](../../../engine/audio/audio_service.h) |
| `ELYSIA_CAMERA` | 相机管理 | [相机工作流](../architecture/subsystems/camera.md) | [camera_manager.h](../../../engine/camera/camera_manager.h) |
| `ELYSIA_USER_CONFIG` | 用户配置管理 | [运行时配置](../architecture/subsystems/runtime-config.md) | [user_config_service.h](../../../engine/config/user_config_service.h) |
| `ELYSIA_TIME` | 时间相关功能 | [时间与计时器](time-and-timers.md) | [time.h](../../../engine/core/time.h) |
| `ELYSIA_EFFECTS` | 特效服务 | [动画与特效](../architecture/subsystems/resources/animation-and-effects.md) | [effect_service.h](../../../engine/effects/effect_service.h) |
| `ELYSIA_LOCALIZATION` | 本地化服务 | [本地化与文本](localization.md) | [localization_service.h](../../../engine/localization/localization_service.h) |
| `ELYSIA_OBJECT_QUERY` | 游戏对象查询 | [游戏对象查询](object-query.md) | [game_object_query_service.h](../../../engine/object_query/game_object_query_service.h) |
| `ELYSIA_RESOURCES` | 资源管理 | [资源加载与配置格式](../architecture/subsystems/resources/README.md) | [resource_service.h](../../../engine/resources/resource_service.h) |
| `ELYSIA_SAVE` | 存档管理 | [存档服务](../architecture/subsystems/save.md) | [save_service.h](../../../engine/save/save_service.h) |
| `ELYSIA_DEBUG_DRAW` | 调试绘制 | [调试绘制](debug-draw.md) | [debug_draw.h](../../../engine/tools/debug_draw.h) |

控制器功能通过 `elysia::gameplay::ControllerService::instance()` 访问，公开接口定义见 [controller_service.h](../../../engine/gameplay/control/controller_service.h)，使用方式见[控制器与控制命令](gameplay/control.md)。

### 日志宏

引擎提供不同级别的日志宏，用于记录运行信息、调试信息、警告与错误。

| 宏名称 | 日志级别 | 用途 |
| --- | --- | --- |
| `ELYSIA_LOG_DEBUG` | Debug | 调试信息 |
| `ELYSIA_LOG_INFO` | Info | 普通运行信息 |
| `ELYSIA_LOG_WARN` | Warn | 警告信息 |
| `ELYSIA_LOG_ERROR` | Error | 错误信息 |

日志分类、流式消息与错误处理约定见[随机数与日志工具](utilities.md)。

基本用法：

```cpp
ELYSIA_LOG_INFO("Gameplay", "Game initialized.");
ELYSIA_LOG_ERROR("Resources", "Failed to load texture.");
```

## 错误处理与恢复

Elysia Engine 提供内置应用错误场景，用于处理无法在当前场景中恢复、但仍允许错误界面正常运行的错误。

当游戏无法继续正常运行时，可以通过引擎提供的接口进入错误场景，展示相关错误信息。

具体使用方式请参阅[应用错误场景](builtin-scenes/application-failure.md)。


## 其他通用工具

除常用服务外，Elysia Engine 还提供了一些可复用的开发工具。

| 工具 | 用途 | 源文件 |
| --- | --- | --- |
| `RandomGenerator` | 随机数生成 | [random_generator.h](../../../engine/tools/random_generator.h) |
| `Timer` | 计时功能 | [timer.h](../../../engine/tools/timer.h) |

随机数的范围、种子和错误条件见[随机数与日志工具](utilities.md)；Timer 的驱动与生命周期见[时间与计时器](time-and-timers.md)。

## 引擎内置场景

Elysia Engine 提供以下内置场景，用于处理常见的应用流程。

| 场景 | 用途 | 使用文档 |
| --- | --- | --- |
| `StartupLoading` | 启动加载与初始化 | [启动加载场景](builtin-scenes/startup-loading.md) |
| `Settings` | 游戏设置界面 | [设置场景](builtin-scenes/settings.md) |
| `ApplicationFailure` | 应用错误的展示与处理 | [应用错误场景](builtin-scenes/application-failure.md) |

场景标识符定义于 [builtin_scene_keys.h](../../../engine/builtin/builtin_scene_keys.h)。

各场景的配置、数据传递方式及使用约定，请参考对应的使用文档、场景定义和 Payload 头文件。

## 进一步阅读

- [架构与模块设计](../architecture/README.md)：了解引擎内部的职责划分与运行机制。
- [开发与贡献指南](../contributing/README.md)：了解代码规范、测试和文档维护要求。
- [设计文档与开发路线](../design/README.md)：查看未来开发计划、设计提案及待完成的架构迁移。
- [返回项目介绍](../../../README.zh-CN.md)：查看项目定位、功能概览与仓库组成。