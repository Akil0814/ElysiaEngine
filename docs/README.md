# Elysia Engine 开发者文档

本目录是 Elysia Engine 当前开发者文档的统一入口。除明确标注的实现指南外，文档应以仓库当前代码、配置和测试为事实来源。

## 开始使用

- [快速开始](getting-started/README.md)：了解依赖、构建、运行和示例集成方式。
- [构建、运行与测试](getting-started/build-and-run.md)：Windows x64 命令、构建产物和测试入口。

## 理解引擎

- [架构总览](architecture/overview.md)：模块边界、依赖方向和运行时生命周期。
- [仓库结构](architecture/repository-layout.md)：源码、资源、测试和示例层的职责。
- [玩家主机联机与输入架构设计](architecture/player-host-networking-and-input-design.md)：目标设计／讨论记录，说明 Player Host 目标与迁移边界；本地输入路由已实施，网络模块尚未实现。
- [子系统索引](subsystems/README.md)：UI、Input、资源加载、配置、音频、相机、存档、Physics、Development Overlay 与 Development Demos。

## 参与维护

- [开发文档](development/README.md)：代码规范、测试约定与文档维护规则。
- [C++ 代码规范](development/coding-standard.md)
- [C++11+ 语言特性盘点](development/cpp-language-features.md)
- [测试指南](development/testing.md)
- [文档维护规则](development/documentation.md)

## 文档状态约定

- 未特别说明的文档描述当前实现。
- 玩家主机联机与输入架构设计是目标设计／讨论记录；其中的拟议接口与流程不代表当前引擎能力，建议方案和待定事项不作为已确认的实现要求。
- Physics 目录是实现指南，其中会明确区分当前类型与目标设计，不能将目标 API 当作已实现接口使用。
- 已废弃的阶段性审查、覆盖矩阵和重构计划不在当前文档中保留。
- 论文和课程发布物位于 `archive/papers/`，不属于当前开发者文档，也不作为当前实现的事实来源。
