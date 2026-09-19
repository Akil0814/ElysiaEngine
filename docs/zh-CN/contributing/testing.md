# Elysia Engine 测试指南

Elysia Engine 使用 CTest 管理自动化测试。测试由根 CMake 工程注册，测试源码位于 `tests/`。

各测试目标直接链接 `engine_lib`。在 Windows 上，部分依赖 SDL 的测试目标会在构建后复制所需的运行时 DLL。

## 运行测试

运行测试前，请先按照[构建、运行与测试](../user-guide/build-and-run.md)完成项目配置。

### 完整测试

构建项目并运行全部已注册测试：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`--output-on-failure` 会在测试失败时输出相关信息，方便定位问题。

### 按子系统运行

测试使用以下 CTest Label 分类：

`application`、`audio`、`bootstrap`、`camera`、`config`、`core`、`effects`、`elysia`、`input`、`loading`、`localization`、`physics`、`save`、`scene`、`tools`、`typography` 和 `ui`。

例如，仅运行物理系统测试：

```powershell
ctest --test-dir build -C Debug -L physics --output-on-failure
```

仅运行 UI 测试：

```powershell
ctest --test-dir build -C Debug -L ui --output-on-failure
```

`-L` 使用正则表达式匹配测试标签。如需严格匹配某个标签，可以使用 `-L "^physics$"`。

### 查看已注册测试

```powershell
ctest --test-dir build -C Debug -N
```

## 测试维护约定

- 新增功能或修改现有行为时，应在对应子系统目录增加或扩展自动化测试。
- Development Demo 可用于人工观察和调试，但不应替代可自动验证的测试。
- 修改公开 API 契约、失败语义或对象生命周期时，应同步更新相关测试与文档。
- 涉及 SDL 的测试应使用现有的 `add_elysia_test(... SDL)` 模式，保持运行时依赖处理方式一致。
- 提交代码前，应运行与改动相关的测试。对于影响多个子系统的改动，建议运行完整测试集。
- 测试结果仅能证明已覆盖环境下的行为。文档中的平台兼容性声明应以实际验证结果为准。