# 构建、运行与测试

## Windows MSVC x64

要求 CMake 3.24+、支持 C++23 的 MSVC，以及支持 SDL3 GPU renderer 的图形驱动。所有 SDL 依赖与选定编解码器均使用仓库固定源码；正常配置和构建不需要网络。版本与补丁见[依赖来源](../../thirdparty/SDL3-DEPENDENCIES.md)。

使用独立目录，不复用旧 SDL2 缓存。以下命令使用 Visual Studio 多配置生成器：

```powershell
cmake -S . -B out/build/sdl3-Debug -A x64 -DELYSIA_ENABLE_IMGUI=ON
cmake --build out/build/sdl3-Debug --config Debug --parallel
ctest --test-dir out/build/sdl3-Debug -C Debug --output-on-failure
cmake -S . -B out/build/sdl3-Release -A x64 -DELYSIA_ENABLE_IMGUI=OFF
cmake --build out/build/sdl3-Release --config Release --parallel
ctest --test-dir out/build/sdl3-Release -C Release --output-on-failure
```

若目录已经使用 Ninja，请保留原生成器，或选择新的目录。在 x64 Native Tools 命令行中使用 `-G Ninja -DCMAKE_BUILD_TYPE=Debug` 或 `Release`，省略 `-A x64`。Ninja 的可执行文件位于构建目录下，无 `Debug/` 子目录。

从仓库根目录运行：

```powershell
.\out\build\sdl3-Debug\Debug\ElysiaEngine.exe
```

SDL3 为共享库，CMake 将实际生成的 SDL3 DLL 复制到程序和测试目录。image、ttf、mixer、gfx 与编解码器静态链接，不需要旧 SDL2 DLL。MSVC 运行库默认使用动态 CRT，Debug 与 Release 必须各自保持一致。

## Dear ImGui

默认 `ELYSIA_ENABLE_IMGUI=ON`，使用 Dear ImGui 1.92.9 的 SDL3 与 SDLRenderer3 后端。关闭时使用独立目录并传入 `-DELYSIA_ENABLE_IMGUI=OFF`。接口说明见 [Development Overlay](../subsystems/development-overlay.md)。

## GPU 与测试

```powershell
ctest --test-dir out/build/sdl3-Debug -C Debug -L gpu --output-on-failure
ctest --test-dir out/build/sdl3-Debug -C Debug -L input --output-on-failure
ctest --test-dir out/build/sdl3-Debug -C Debug -L ui --output-on-failure
```

GPU 测试需要真实图形设备，不能使用 dummy video 驱动。`sdl3_gpu_integration_tests` 会验证无效 GPU 后端的失败路径、实际 GPU 绘制和读回，并在测试工作目录写入 `sdl3-gpu-verification.png`。示例应用 smoke test 会运行数秒后注入正常退出事件。软件 renderer 测试用于精确检查 UI 像素规则，不能代替 GPU 测试。

无显示环境可用 `ctest -LE gpu` 运行其余测试，但这不满足完整迁移验收。SDL3 的驱动环境变量名是 `SDL_VIDEO_DRIVER` 和 `SDL_AUDIO_DRIVER`。

## Linux、macOS 与 MinGW

各平台使用相同的 vendored 源码，不再查找系统 SDL2 开发包。需要 C++23 工具链、CMake、平台图形/音频开发库及可用 GPU 驱动。SDL 的平台系统依赖见仓库中的 [Linux 说明](../../thirdparty/SDL3/docs/README-linux.md) 与 [macOS 说明](../../thirdparty/SDL3/docs/README-macos.md)。

```bash
cmake -S . -B out/build/sdl3-linux -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/sdl3-linux --parallel
ctest --test-dir out/build/sdl3-linux --output-on-failure
```

macOS 或 MinGW 使用各自的新目录与工具链运行同样的配置入口。当前这些平台尚未实机验证；不能将 Windows 构建结果视为其他平台的验收结果。

## 常见问题

- GPU 创建失败：检查系统图形驱动与 SDL GPU 后端支持。应用会报告错误并清理资源，不自动退回软件 renderer。
- 找不到资源：从仓库根目录启动，保留 `assets/` 的目录结构。
- Debug 并行构建遇到 MSVC PDB 冲突：使用新的构建目录，可在 Ninja 配置时设置 `-DCMAKE_C_FLAGS_DEBUG="/Od /Z7 /RTC1" -DCMAKE_CXX_FLAGS_DEBUG="/Od /Z7 /RTC1"`。
- 不同生成器或架构不能共用缓存，切换时使用独立构建目录。

