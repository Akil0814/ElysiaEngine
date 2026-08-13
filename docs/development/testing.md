# 测试指南

测试由根 CMake 工程通过 CTest 注册，源码位于 `tests/`。每个目标直接链接 `engine_lib`，部分依赖 SDL 的目标会在 Windows 构建后复制所需运行时 DLL。

## 完整测试

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## 按子系统运行

测试使用 `application`、`audio`、`bootstrap`、`camera`、`config`、`core`、`effects`、`elysia`、`input`、`loading`、`localization`、`physics`、`save`、`scene`、`tools`、`typography` 和 `ui` 等 label。

```powershell
ctest --test-dir build -C Debug -L physics --output-on-failure
ctest --test-dir build -C Debug -L ui --output-on-failure
```

## 维护约定

- 新行为应在对应子系统目录增加或扩展测试，而不是依赖 Development Demo 的人工观察。
- 修改公开契约、失败语义或生命周期时，同步更新相关文档。
- 涉及 SDL 的测试应使用现有 `add_elysia_test(... SDL)` 模式，保持运行时依赖复制规则一致。
- 测试不能证明未覆盖平台的兼容性；文档只声明实际验证过的环境。
