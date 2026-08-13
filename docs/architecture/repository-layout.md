# 仓库结构与依赖边界

```text
ElysiaEngine/
├─ engine/       可复用引擎代码
├─ game/         示例项目集成层
├─ assets/       运行时配置与资源
├─ tests/        自动化测试
├─ docs/         当前开发者文档
├─ archive/      不作为当前事实来源的发布物
├─ thirdparty/   第三方头文件、库和 DLL
├─ main.cpp      示例程序入口
└─ CMakeLists.txt
```

## `engine/`

按职责拆分为 application、bootstrap、scene、input、resources、loading、config、audio、camera、ui、physics、save、effects 等模块。生产子系统之间应通过明确的类型或服务边界协作，不得反向依赖 `game/` 或 Development Demo 专用代码。

## `game/`

这里不是独立后的旧游戏项目，而是可运行的示例集成层：

- `application/` 实现 `IGameModule`；
- `scene/` 提供示例场景和 `example::scene_keys`；
- `game_lib` 公开依赖 `engine_lib`。

真实项目可以替换这一层，而不修改引擎核心。

## `assets/`

包含引擎和示例程序运行所需的配置、字体、音频、纹理、本地化和预加载资源。资源路径及 JSON schema 见[资源加载文档](../subsystems/resources/README.md)。

## `tests/`

测试目标由 CTest 注册并按子系统设置 label。测试代码直接链接 `engine_lib`，不依赖示例游戏逻辑。详细命令见[测试指南](../development/testing.md)。

## `docs/` 与 `archive/`

`docs/` 只保存当前开发者文档和明确标识的实现指南。`archive/` 中的论文等发布物保留其历史表述，但不用于判断当前 API 或实现状态。
