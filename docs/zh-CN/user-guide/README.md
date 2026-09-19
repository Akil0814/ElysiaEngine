# Elysia Engine 使用指南

本指南面向使用 Elysia Engine 开发游戏的开发者，帮助你完成环境配置、构建运行，并查找游戏开发中需要的公开接口、资源格式与使用流程。

## 开始使用

从[构建、运行与测试](build-and-run.md)开始，了解工具链要求、构建命令、运行目录、测试方式与常见问题。

## 共同必读

[游戏开发必读：核心概念](core-concepts.md)介绍场景、游戏对象与 UI 的职责、生命周期和执行时机。无论负责的任务大小，都应先掌握这些基础，再按开发任务查阅下方专题。

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

## 进一步阅读

- [架构与模块设计](../architecture/README.md)：了解引擎内部的职责划分与运行机制。
- [开发与贡献指南](../contributing/README.md)：了解代码规范、测试和文档维护要求。
- [返回项目介绍](../../../README.zh-CN.md)：查看项目定位、功能概览与仓库组成。
