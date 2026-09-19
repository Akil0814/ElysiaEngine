<div align="center">
  <img src="assets/engine/textures/elysia.png" alt="Elysia Engine" width="300">
  <h1>Elysia Engine</h1>
</div>

[English](README.md) | **[简体中文](README.zh-CN.md)** | [日本語](README.ja.md)

Elysia Engine 是一个使用 C++23 与 SDL3 构建的模块化二维游戏引擎与运行时框架，为游戏提供应用生命周期、场景路由、资源管理、输入、渲染、音频、UI、物理与存档等通用能力。

本仓库同时包含可复用的引擎实现和示例程序：`engine/` 承载引擎能力，`game/` 展示游戏模块与引擎的集成，构建得到的 `ElysiaEngine` 可执行文件运行该示例程序。引擎实现与游戏业务保持独立，便于分别扩展与维护。

## 核心功能与当前状态

- **应用与场景**：应用生命周期、场景注册与路由，以及游戏模块接入边界。
- **资源与配置**：资源清单（manifest）、内容注册、运行时配置、国际化与用户配置服务。
- **输入与 Gameplay**：原始设备输入、动作映射、玩家控制器、固定 tick 命令消费和场景输入路由。
- **渲染与 UI**：SDL3 GPU renderer、保留模式 UI、布局、焦点、窗口表面、样式与控件。
- **音频与相机**：音频资源播放调度、相机槽位、跟随、震屏与坐标投影。
- **物理与存档**：Box2D 集成、碰撞与 Gameplay 运行时，以及类型化存档和可靠恢复。
- **动画与特效**：动画资源、图集与 `EffectDefinition` 配置。
- **开发支持**：Demo Gallery、可选 Dear ImGui 开发覆盖层，以及按子系统组织的单元和集成测试。

**网络能力边界**：ENet 当前仅作为引擎链接的传输层依赖保留，网络会话、状态同步和网络联机功能尚未实现。

## 快速开始

### 环境要求

以下流程使用 Windows、MSVC x64 和 Visual Studio 多配置生成器，需要：

- CMake 3.24 或更高版本。
- 支持 C++23 的 MSVC 工具链。
- 支持 SDL3 GPU renderer 的图形设备与驱动。GPU renderer 创建失败时，应用会报告错误并退出，不自动回退到软件渲染。

SDL3、image、ttf、mixer、gfx 及所需编解码器均使用仓库内固定源码构建，正常配置和构建不下载依赖。具体版本与本地补丁见[依赖来源](thirdparty/SDL3-DEPENDENCIES.md)。

### 构建与运行

在仓库根目录执行：

```powershell
cmake -S . -B out/build/sdl3-Debug -A x64
cmake --build out/build/sdl3-Debug --config Debug --parallel
.\out\build\sdl3-Debug\Debug\ElysiaEngine.exe
```

运行时请保持工作目录为仓库根目录，并保留 `assets/` 的目录结构，以便正确加载资源。

Dear ImGui 开发覆盖层默认启用。关闭方法、Release 配置和 Ninja 构建方式见[构建、运行与测试](docs/zh-CN/user-guide/build-and-run.md)。

> 从旧版迁移时，不要复用 SDL2 构建缓存，请使用新的 SDL3 构建目录。切换生成器或架构时也应使用独立目录。

### 运行测试

完成构建后执行：

```powershell
ctest --test-dir out/build/sdl3-Debug -C Debug --output-on-failure
```

GPU 测试需要真实图形设备，不能使用 dummy video 驱动。按标签运行测试、无显示环境的测试方式及故障排查见[构建、运行与测试](docs/zh-CN/user-guide/build-and-run.md)。

### 其他平台

Linux、macOS 和 MinGW 使用相同的仓库固定源码依赖，但当前尚未实机验证，不能将 Windows 构建结果视为其他平台的验证结果。对应工具链和系统依赖要求见[详细构建说明](docs/zh-CN/user-guide/build-and-run.md)。

## 文档导航

| 阅读目的 | 文档入口 |
| --- | --- |
| 配置环境、构建运行与排查问题 | [构建、运行与测试](docs/zh-CN/user-guide/build-and-run.md) |
| 查找使用引擎与公开接口的说明 | [使用指南](docs/zh-CN/user-guide/README.md) |
| 了解子系统边界与运行机制 | [架构与模块设计](docs/zh-CN/architecture/README.md) |
| 参与开发，了解代码规范、测试与文档维护 | [开发与贡献指南](docs/zh-CN/contributing/README.md) |
| 查看计划中的能力、技术讨论与迁移设计 | [设计与路线图](docs/zh-CN/design/README.md) |
| 浏览开发者文档分类 | [开发者文档总入口](docs/zh-CN/README.md) |

## 仓库组成与依赖边界

- `engine/`：可复用的引擎实现，构建为 `engine_lib`。
- `game/`：示例集成层，提供游戏模块与示例场景，构建为 `game_lib`。
- `assets/`：示例程序和引擎内建流程使用的配置、字体、音频与纹理资源。
- `tests/`：按子系统组织的单元测试和集成测试。
- `docs/`：使用指南、架构说明、贡献指南与设计文档。
- `thirdparty/`：仓库随附的第三方依赖。

主要依赖方向为：

```text
ElysiaEngine executable -> game_lib -> engine_lib
                                      -> SDL3 libraries
                                      -> Box2D
                                      -> ENet
```

引擎层不依赖 `game/`。游戏业务通过 `IGameModule` 提供应用描述和场景注册，业务类型由游戏模块维护。

## 基于引擎的项目与历史

### 基于引擎的项目

| 项目 | 简介 | 状态 |
| --- | --- | --- |
| [Codex Zero](https://github.com/ZacharyOllivierre/Codex-Zero) | 基于该引擎的 2D 俯视角多人射击游戏 | 开发中 |
| [Moonline](https://github.com/Akil0814/Moonline) | 基于 MELTY BLOOD 中出现的角色开发的 2D 横版战斗游戏 | 停滞中 |

### 项目历史

- 引擎早期开发提交：[Moonline](https://github.com/Akil0814/Moonline)。
- 早期原型开发：[Project-Hail-Mary](https://github.com/ZacharyOllivierre/Project-Hail-Mary)。

## 许可与版权

本项目代码使用 [MIT License](LICENSE)。随附第三方源码的许可与声明见 [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md)，对应内容遵循各自的许可条款。

Elysia 图标版权归属 © miHoYo 所有，不属于本项目 MIT 许可的授权范围。

Elysia Engine 为独立开发项目，与 miHoYo 无官方关联，也未获得其官方认可或赞助。
