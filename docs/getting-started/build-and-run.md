# 构建、运行与测试

## Windows x64

要求：

- CMake 3.22 或更高版本；
- 支持 C++23 的 Visual Studio/MSVC 工具链；
- x64 生成器平台。

仓库已经在 `thirdparty/SDL2` 中提供 MSVC x64 的 SDL2、SDL2_image、SDL2_mixer、SDL2_ttf 和 SDL2_gfx 头文件、导入库与运行时 DLL。ENet 1.3.18 由 `thirdparty/enet` 的固定源码静态构建，不需要额外运行时 DLL。配置阶段会拒绝 Win32 生成器。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

运行示例程序：

```powershell
.\build\Debug\ElysiaEngine.exe
```

对于 Visual Studio 多配置生成器，Debug 测试需要显式指定配置：

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

可以按标签运行单个子系统，例如：

```powershell
ctest --test-dir build -C Debug -L input --output-on-failure
ctest --test-dir build -C Debug -L ui --output-on-failure
```

## 可选 Dear ImGui 开发覆盖层

当前开发构建默认启用 Dear ImGui。需要修正旧 CMake 缓存或建立独立开发目录时，可以显式开启：

```powershell
cmake -S . -B build-imgui -A x64 -DELYSIA_ENABLE_IMGUI=ON
cmake --build build-imgui --config Debug
ctest --test-dir build-imgui -C Debug -L tools --output-on-failure
```

具体生命周期、输入捕获和面板注册契约见 [Development Overlay](../subsystems/development-overlay.md)。

发布构建应显式使用 `-DELYSIA_ENABLE_IMGUI=OFF`，从产物中完全移除 ImGui 与 Overlay 逐帧接入点。

## macOS

macOS 配置会在 `/opt/homebrew` 和 `/usr/local` 中查找 SDL2、SDL2_image、SDL2_mixer 和 SDL2_ttf；ENet 仍由仓库内的源码构建。安装依赖后可使用单配置构建：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

该路径存在于当前 CMake 配置中，但本仓库不将 macOS 声明为与 Windows x64 同等级的持续验证平台。

## Linux（Ubuntu 24.04 / WSL2）

目标环境为 Ubuntu 24.04 x64 原生桌面或 WSL2，计划验证的工具链为 GCC 13。当前已补齐构建配置，但尚无 Linux 实机构建通过记录。

在 Ubuntu 终端安装构建工具和系统 SDL2 开发包：

```bash
sudo apt update
sudo apt install build-essential gcc-13 g++-13 cmake ninja-build pkg-config \
  libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev libsdl2-gfx-dev
```

Linux 通过 `pkg-config` 查找上述五个 SDL 模块，缺少任意一个会在 CMake 配置阶段报错。ENet、Box2D 和 ImGui 由仓库源码构建，无需单独安装；不使用 `thirdparty/SDL2/lib` 中的 Windows 二进制，也不需要 SDL_net。

在仓库根目录执行以下命令。Linux 使用独立的 `build-linux` 目录，不复用 Windows 的 CMake 缓存；ImGui 默认开启：

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc-13 \
  -DCMAKE_CXX_COMPILER=g++-13
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
```

在没有显示或音频设备的环境中，仅为测试进程设置 dummy 驱动：

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ctest --test-dir build-linux --output-on-failure
```

这些变量不应设为游戏运行的默认环境。交互运行需要 Linux 图形桌面或 WSLg，并从仓库根目录启动：

```bash
./build-linux/ElysiaEngine
```

后续 Linux 实机验收需要完成 Debug 全量构建、完整 CTest、`ELYSIA_ENABLE_IMGUI=OFF` 构建，以及窗口、输入和音频的交互检查。无显示测试不能代替交互验证。

## 常见问题

- 配置提示 SDL 库架构不匹配：删除错误架构的构建目录，使用 `-A x64` 重新配置。
- 程序启动时找不到资源：从仓库根目录启动构建产物，确保 `assets/` 保持原有相对位置。
- 修改了源码但目标未更新：重新运行 CMake 配置；`engine/` 与 `game/` 使用 `GLOB_RECURSE CONFIGURE_DEPENDS` 收集 `.cpp`。
- 只构建不运行测试不能验证数据文件和运行时集成；提交前应运行完整 CTest。
