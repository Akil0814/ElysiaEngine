# 构建、运行与测试

## Windows x64

要求：

- CMake 3.20 或更高版本；
- 支持 C++23 的 Visual Studio/MSVC 工具链；
- x64 生成器平台。

仓库已经在 `thirdparty/SDL2` 中提供 MSVC x64 的 SDL2、SDL2_image、SDL2_net、SDL2_mixer、SDL2_ttf 和 SDL2_gfx 头文件、导入库与运行时 DLL。配置阶段会拒绝 Win32 生成器。

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

## macOS

macOS 配置会在 `/opt/homebrew` 和 `/usr/local` 中查找 SDL2、SDL2_image、SDL2_net、SDL2_mixer 和 SDL2_ttf。安装依赖后可使用单配置构建：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

该路径存在于当前 CMake 配置中，但本仓库不将 macOS 声明为与 Windows x64 同等级的持续验证平台。

## 常见问题

- 配置提示 SDL 库架构不匹配：删除错误架构的构建目录，使用 `-A x64` 重新配置。
- 程序启动时找不到资源：从仓库根目录启动构建产物，确保 `assets/` 保持原有相对位置。
- 修改了源码但目标未更新：重新运行 CMake 配置；`engine/` 与 `game/` 使用 `GLOB_RECURSE CONFIGURE_DEPENDS` 收集 `.cpp`。
- 只构建不运行测试不能验证数据文件和运行时集成；提交前应运行完整 CTest。
