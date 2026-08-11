# Elysia Engine

Elysia Engine 是一个使用 C++23 与 SDL2 构建的二维游戏引擎。仓库同时包含可复用的引擎库、一个用于演示集成方式的示例游戏层，以及覆盖主要子系统的自动化测试。

当前项目仍处于开发阶段，适合用于研究和构建二维游戏运行时；它不是已经稳定发布的通用游戏引擎 SDK。现有实现包括应用生命周期、场景路由、输入映射、资源与配置加载、音频、相机、UI、存档、动画与特效等能力。物理模块已经提供基础契约和策略接口，但完整的场景级模拟闭环仍在设计与实现中。

## 仓库组成

- `engine/`：可复用的引擎实现，构建为 `engine_lib`。
- `game/`：示例集成层，展示如何实现 `IGameModule`、注册场景并选择初始路由，构建为 `game_lib`。
- `assets/`：示例程序和引擎内建流程使用的配置、字体、音频与纹理资源。
- `tests/`：按子系统组织的单元测试和集成测试。
- `docs/`：当前开发者文档的统一入口。
- `thirdparty/`：仓库随附的第三方依赖。

依赖方向保持为：

```text
ElysiaEngine executable -> game_lib -> engine_lib -> SDL2
```

引擎层不依赖 `game/`。实际项目通过 `IGameModule` 提供应用描述和场景注册，而不是把业务类型加入引擎。

## 构建与运行

当前明确验证的平台是 Windows x64。需要 CMake 3.20 或更高版本，以及支持 C++23 的 MSVC 工具链。Windows 构建使用仓库 `thirdparty/SDL2` 中随附的 x64 库。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
.\build\Debug\ElysiaEngine.exe
```

运行测试：

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

macOS 的 CMake 分支会从 Homebrew 常用路径查找 SDL2、SDL2_image、SDL2_net、SDL2_mixer 和 SDL2_ttf，但本仓库不将其列为与 Windows 同等级的已验证平台。当前 CMake 没有完整配置 Linux 依赖查找流程。

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
