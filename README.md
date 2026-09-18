# Elysia Engine

Elysia Engine 是一个使用 C++23 与 SDL3 构建的二维游戏引擎。仓库同时包含可复用的引擎库、一个用于演示集成方式的示例游戏层，以及覆盖主要子系统的自动化测试。

当前项目仍处于开发阶段，适合用于研究和构建二维游戏运行时；它不是已经稳定发布的通用游戏引擎 SDK。现有实现包括应用生命周期、场景路由、输入映射、资源与配置加载、音频、相机、UI、存档、动画与特效等能力。物理模块已经提供基础契约和策略接口，但完整的场景级模拟闭环仍在设计与实现中。

引擎早期开发提交位于仓库: [Moonline](https://github.com/Akil0814/Moonline)

## 基于引擎的项目

### [Codex Zero](https://github.com/ZacharyOllivierre/Codex-Zero)

基于该引擎的2D俯视角多人射击游戏 ---开发中

### [Moonline](https://github.com/Akil0814/Moonline)

基于MELTY BLOOD中出现角色开发的2D横板战斗游戏 ---停滞中

## 仓库组成

- `engine/`：可复用的引擎实现，构建为 `engine_lib`。
- `game/`：示例集成层，展示如何实现 `IGameModule`、注册场景并选择初始路由，构建为 `game_lib`。
- `assets/`：示例程序和引擎内建流程使用的配置、字体、音频与纹理资源。
- `tests/`：按子系统组织的单元测试和集成测试。
- `docs/`：当前开发者文档的统一入口。
- `thirdparty/`：仓库随附的第三方依赖。

依赖方向保持为：

```text
ElysiaEngine executable -> game_lib -> engine_lib
                                      -> SDL3 libraries
                                      -> Box2D
                                      -> ENet
```

引擎层不依赖 `game/`。实际项目通过 `IGameModule` 提供应用描述和场景注册，而不是把业务类型加入引擎。

## 构建与运行

当前主要验收平台是 Windows MSVC x64，需要 CMake 3.24 或更高版本和 C++23 工具链。SDL3、image、ttf、mixer、gfx 及所需编解码器均使用仓库内固定源码构建，配置和构建不下载依赖。渲染使用 SDL3 GPU renderer，创建失败时明确退出。

```powershell
cmake -S . -B out/build/sdl3-Debug -A x64
cmake --build out/build/sdl3-Debug --config Debug
ctest --test-dir out/build/sdl3-Debug -C Debug --output-on-failure
.\out\build\sdl3-Debug\Debug\ElysiaEngine.exe
```

请使用新的 SDL3 构建目录。Linux、macOS 和 MinGW 使用相同的固定源码依赖，其实际构建和交互验收尚未验证。版本和本地补丁见[依赖来源](thirdparty/SDL3-DEPENDENCIES.md)，迁移验收记录见[SDL3 迁移](docs/development/sdl3-migration.md)。

更完整的环境与故障排查说明见[构建、运行与测试](docs/getting-started/build-and-run.md)。

## 文档

- [开发者文档总入口](docs/README.md)
- [快速开始](docs/getting-started/README.md)
- [引擎架构](docs/architecture/overview.md)
- [子系统文档](docs/subsystems/README.md)
- [开发与维护](docs/development/README.md)

物理模块文档是一组实现指南，包含当前审计、目标架构与实施路线；其中描述的目标类型和流程不代表已经全部落地。

## License

本项目使用 [MIT License](LICENSE.txt)。
