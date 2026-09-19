<div align="center">
  <img src="assets/engine/textures/elysia.png" alt="Elysia Engine" width="300">
  <h1>Elysia Engine</h1>
</div>

Elysia Engine 是一个使用 C++23 与 SDL3 构建的模块化二维游戏引擎与运行时框架。它提供应用生命周期、场景路由、资源与配置加载、输入、音频、相机、UI、存档、动画与特效等通用能力。

## 核心功能


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

当前项目已通过 Windows MSVC/GCC MacOS编译测试，环境需要 CMake 3.24 或更高版本和 C++23 工具链。SDL3、image、ttf、mixer、gfx 及所需编解码器均使用仓库内固定源码构建，配置和构建不下载依赖。渲染使用 SDL3 GPU renderer，创建失败时明确退出。

```powershell
cmake -S . -B out/build/sdl3-Debug -A x64
cmake --build out/build/sdl3-Debug --config Debug
ctest --test-dir out/build/sdl3-Debug -C Debug --output-on-failure
.\out\build\sdl3-Debug\Debug\ElysiaEngine.exe
```

请使用新的 SDL3 构建目录。Linux、macOS 和 MinGW 使用相同的固定源码依赖。版本和本地补丁见[依赖来源](thirdparty/SDL3-DEPENDENCIES.md).

更完整的环境与故障排查说明见[构建、运行与测试](docs/getting-started/build-and-run.md)。

## 文档

- [开发者文档总入口](docs/README.md)


## 历史版本

引擎早期开发提交位于仓库: [Moonline](https://github.com/Akil0814/Moonline)

早期原型开发位于仓库: [Project-Hail-Mary](https://github.com/ZacharyOllivierre/Project-Hail-Mary)

## License

本项目使用 [MIT License](LICENSE)。

第三方源码声明见 [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md)。

Elysia 图标版权归属 © miHoYo 所有

Elysia Engine 为独立开发项目，与 miHoYo 无官方关联，也未获得其官方认可或赞助。
Elysia Engine is an independent project and is not affiliated with or endorsed by miHoYo.
