# C++11–23 语言与标准库特性统计

此项目中大量使用了现代C++语法。

本页盘点 Elysia Engine 第一方源码中可重复识别的现代 C++ 用法。项目通过
[CMake 配置](../../CMakeLists.txt)要求 C++23。

## 范围与计数口径

- 扫描 `engine/`、`game/`、`tests/` 中的 `.h`、`.hpp`、`.cpp` 文件。
- 排除 `thirdparty/`、构建目录、文档、脚本和生成产物。
- 扫描器先隐藏注释、普通/原始字符串和字符字面量，再匹配注册过的语法或标准库符号。
- `engine`、`game`、`tests` 列表示各目录中的模式命中数；“文件数”是至少命中一次的文件数。
- 同一行可以同时体现多项特性，例如 `constexpr auto` 会分别计入 `constexpr` 和 `auto`。
- 数字表示词法/语法模式规模，不表示运行时调用次数、性能权重或代码质量。

## 当前统计

<!-- BEGIN GENERATED: cpp-language-feature-stats -->

本区块由 `scripts/audit_cpp_language_features.ps1` 根据当前工作树生成；共扫描 628 个第一方 C++ 源文件。
“命中”表示过滤注释及字符串/字符字面量后的检测模式次数，不等同于运行时调用次数。

### C++11

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp11-array` | 标准库 | `std::array` | 37 | 10 | 44 | 91 | 46 | [源码](../../game/scene/demo/ui_component_gallery_scene.h) |
| `cpp11-auto` | 语言 | `auto` 类型推导 | 972 | 222 | 787 | 1981 | 212 | [源码](../../game/scene/main_menu_scene.cpp) |
| `cpp11-chrono` | 标准库 | `std::chrono` | 10 | 0 | 5 | 15 | 8 | [源码](../../tests/audio/sound_playback_scheduler_tests.cpp) |
| `cpp11-concurrency` | 标准库 | 线程、同步与原子操作 | 46 | 0 | 6 | 52 | 9 | [源码](../../engine/tools/termination_manager.h) |
| `cpp11-constexpr` | 语言 | `constexpr` | 500 | 33 | 48 | 581 | 115 | [源码](../../game/scene/demo/engine_feature_lab_scene.cpp) |
| `cpp11-decltype` | 语言 | `decltype` | 15 | 1 | 3 | 19 | 12 | [源码](../../tests/camera/camera_manager_tests.cpp) |
| `cpp11-declval` | 标准库 | `std::declval` | 0 | 0 | 1 | 1 | 1 | [源码](../../tests/camera/camera_manager_tests.cpp) |
| `cpp11-defaulted-functions` | 语言 | 显式默认函数 | 150 | 4 | 0 | 154 | 120 | [源码](../../game/scene/main_menu_scene.h) |
| `cpp11-deleted-functions` | 语言 | 显式删除函数 | 74 | 2 | 0 | 76 | 24 | [源码](../../engine/camera/camera_manager.h) |
| `cpp11-enum-class` | 语言 | 有作用域枚举 | 129 | 2 | 3 | 134 | 91 | [源码](../../engine/animation/animation_registration_failure.h) |
| `cpp11-final` | 语言 | `final` | 49 | 22 | 53 | 124 | 83 | [源码](../../game/scene/demo/ui_component_gallery_scene.h) |
| `cpp11-fixed-width-integers` | 标准库 | 定宽整数类型 | 280 | 5 | 50 | 335 | 85 | [源码](../../engine/audio/sound_playback_types.h) |
| `cpp11-function` | 标准库 | `std::function` | 47 | 2 | 8 | 57 | 41 | [源码](../../engine/animation/animation.h) |
| `cpp11-initializer-list` | 标准库 | `std::initializer_list` | 9 | 0 | 1 | 10 | 9 | [源码](../../engine/resources/atlas/atlas.h) |
| `cpp11-lambda` | 语言 | Lambda 表达式 | 269 | 57 | 169 | 495 | 117 | [源码](../../game/scene/demo/engine_feature_lab_scene.cpp) |
| `cpp11-math-functions` | 标准库 | C++11 数学函数 | 118 | 0 | 3 | 121 | 31 | [源码](../../engine/application/application.cpp) |
| `cpp11-move-forward` | 标准库 | 移动与完美转发工具 | 716 | 152 | 146 | 1014 | 171 | [源码](../../game/scene/main_menu_scene.cpp) |
| `cpp11-new-algorithms` | 标准库 | C++11 非修改序列算法 | 16 | 0 | 3 | 19 | 9 | [源码](../../engine/ui/window/ui_window.cpp) |
| `cpp11-noexcept` | 语言 | `noexcept` | 2980 | 123 | 69 | 3172 | 339 | [源码](../../engine/scene/scene_manager.h) |
| `cpp11-nullptr` | 语言 | `nullptr` | 625 | 51 | 301 | 977 | 199 | [源码](../../engine/scene/scene_manager.h) |
| `cpp11-override` | 语言 | `override` | 376 | 84 | 156 | 616 | 110 | [源码](../../game/scene/demo/ui_component_gallery_scene.h) |
| `cpp11-random` | 标准库 | 随机数工具 | 2 | 0 | 3 | 5 | 3 | [源码](../../engine/tools/random_generator.cpp) |
| `cpp11-range-for` | 语言 | 基于范围的 `for` | 314 | 25 | 75 | 414 | 135 | [源码](../../game/scene/demo/ui_component_gallery_content.cpp) |
| `cpp11-smart-pointers` | 标准库 | 智能指针与 `std::make_shared` | 163 | 37 | 19 | 219 | 87 | [源码](../../engine/ui/core/ui_child_host.h) |
| `cpp11-static-assert` | 语言 | `static_assert` | 10 | 0 | 49 | 59 | 14 | [源码](../../engine/scene/scene_manager.h) |
| `cpp11-string-conversion` | 标准库 | `std::to_string` | 25 | 3 | 25 | 53 | 30 | [源码](../../game/demo/physics/demo_combat.cpp) |
| `cpp11-system-error` | 标准库 | 系统错误码工具 | 48 | 0 | 7 | 55 | 19 | [源码](../../engine/builtin/resources/builtin_asset_catalog.cpp) |
| `cpp11-time-formatting` | 标准库 | `std::put_time` | 2 | 0 | 0 | 2 | 1 | [源码](../../engine/tools/logger.cpp) |
| `cpp11-tuple` | 标准库 | 元组工具 | 5 | 0 | 0 | 5 | 3 | [源码](../../engine/ui/core/ui_child_host.h) |
| `cpp11-type-alias` | 语言 | `using` 类型别名 | 113 | 3 | 8 | 124 | 78 | [源码](../../engine/animation/animation.h) |
| `cpp11-type-index` | 标准库 | `std::type_index` | 7 | 0 | 0 | 7 | 3 | [源码](../../engine/ui/style/ui_theme_style_resolver.h) |
| `cpp11-unordered-containers` | 标准库 | 无序容器与哈希 | 70 | 3 | 1 | 74 | 38 | [源码](../../engine/builtin/resources/builtin_asset_cache.h) |
| `cpp11-variadic-templates` | 语言 | 可变参数模板 | 5 | 1 | 0 | 6 | 4 | [源码](../../engine/ui/core/ui_child_host.h) |

### C++14

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp14-chrono-literals` | 标准库 | 时间字面量 | 0 | 0 | 1 | 1 | 1 | [源码](../../tests/audio/sound_playback_scheduler_tests.cpp) |
| `cpp14-generic-lambda` | 语言 | 泛型 Lambda | 41 | 2 | 14 | 57 | 28 | [源码](../../game/scene/demo/physics/physics_combat_demo_scene_base.cpp) |
| `cpp14-init-capture` | 语言 | Lambda 初始化捕获 | 1 | 1 | 0 | 2 | 2 | [源码](../../engine/ui/widgets/ui_button.cpp) |
| `cpp14-make-unique` | 标准库 | `std::make_unique` | 73 | 113 | 151 | 337 | 49 | [源码](../../engine/animation/animation_service.cpp) |
| `cpp14-type-trait-aliases` | 标准库 | 类型特征 `_t` 别名 | 14 | 1 | 2 | 17 | 9 | [源码](../../engine/ui/core/ui_child_host.h) |

### C++17

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp17-any` | 标准库 | `std::any` | 2 | 0 | 1 | 3 | 2 | [源码](../../tests/scene/scene_core_tests.cpp) |
| `cpp17-as-const` | 标准库 | `std::as_const` | 3 | 0 | 0 | 3 | 1 | [源码](../../engine/object_query/game_object_query_service.h) |
| `cpp17-clamp` | 标准库 | `std::clamp` | 59 | 1 | 0 | 60 | 30 | [源码](../../engine/camera/camera_controller.cpp) |
| `cpp17-ctad` | 语言 | 类模板实参推导 | 0 | 0 | 27 | 27 | 16 | [源码](../../tests/application/application_presentation_settings_tests.cpp) |
| `cpp17-filesystem` | 标准库 | `std::filesystem` | 376 | 0 | 310 | 686 | 122 | [源码](../../engine/io/path/path_manager.h) |
| `cpp17-if-constexpr` | 语言 | `if constexpr` | 43 | 1 | 0 | 44 | 12 | [源码](../../engine/camera/camera_manager.cpp) |
| `cpp17-if-initializer` | 语言 | `if` 初始化语句 | 110 | 0 | 0 | 110 | 46 | [源码](../../engine/animation/runtime/animation_manager.cpp) |
| `cpp17-inline-variables` | 语言 | 内联变量 | 227 | 17 | 0 | 244 | 18 | [源码](../../engine/core/render/colors.h) |
| `cpp17-invoke` | 标准库 | `std::invoke` 与结果类型特征 | 7 | 0 | 0 | 7 | 1 | [源码](../../engine/object_query/game_object_query_service.h) |
| `cpp17-nested-namespace` | 语言 | 嵌套命名空间定义 | 535 | 64 | 58 | 657 | 567 | [源码](../../engine/animation/animation.h) |
| `cpp17-nodiscard` | 语言 | `[[nodiscard]]` | 1567 | 80 | 45 | 1692 | 294 | [源码](../../engine/animation/animation.h) |
| `cpp17-optional` | 标准库 | `std::optional` / `std::nullopt` | 479 | 14 | 17 | 510 | 142 | [源码](../../engine/animation/animation.h) |
| `cpp17-scoped-lock` | 标准库 | `std::scoped_lock` | 7 | 0 | 0 | 7 | 1 | [源码](../../engine/config/config_service.cpp) |
| `cpp17-size` | 标准库 | `std::size` | 0 | 0 | 1 | 1 | 1 | [源码](../../tests/loading/resource_load_plan_validator_tests.cpp) |
| `cpp17-string-view` | 标准库 | `std::string_view` | 472 | 8 | 72 | 552 | 142 | [源码](../../engine/animation/animation_service.h) |
| `cpp17-structured-bindings` | 语言 | 结构化绑定 | 20 | 2 | 4 | 26 | 19 | [源码](../../game/demo/physics/demo_combat.cpp) |
| `cpp17-type-trait-variables` | 标准库 | 类型特征 `_v` 变量模板 | 40 | 1 | 11 | 52 | 18 | [源码](../../engine/camera/camera_manager.cpp) |
| `cpp17-variant` | 标准库 | 变体类型工具 | 54 | 1 | 9 | 64 | 27 | [源码](../../engine/ui/widgets/ui_button.cpp) |

### C++20

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp20-bit-operations` | 标准库 | 位操作工具 | 1 | 0 | 0 | 1 | 1 | [源码](../../engine/tools/debug_draw.cpp) |
| `cpp20-concepts` | 语言 | Concepts 与 `requires` 子句 | 14 | 0 | 0 | 14 | 3 | [源码](../../engine/save/save_data.h) |
| `cpp20-designated-initializers` | 语言 | 指定初始化器 | 76 | 57 | 184 | 317 | 77 | [源码](../../game/scene/main_menu_scene.cpp) |
| `cpp20-erase` | 标准库 | `std::erase` / `std::erase_if` | 27 | 0 | 0 | 27 | 12 | [源码](../../engine/physics/physics_world.cpp) |
| `cpp20-ordering-types` | 标准库 | 比较类别类型 | 4 | 0 | 0 | 4 | 2 | [源码](../../engine/physics/collision/collision_target.h) |
| `cpp20-ranges` | 标准库 | Ranges 库 | 56 | 1 | 2 | 59 | 11 | [源码](../../engine/physics/physics_world.cpp) |
| `cpp20-remove-cvref` | 标准库 | `std::remove_cvref_t` | 19 | 0 | 0 | 19 | 6 | [源码](../../engine/save/save_service.h) |
| `cpp20-source-location` | 标准库 | `std::source_location` | 140 | 0 | 6 | 146 | 50 | [源码](../../engine/application/lifecycle/application_termination_logging.h) |
| `cpp20-span` | 标准库 | `std::span` | 74 | 8 | 15 | 97 | 46 | [源码](../../engine/physics/contracts/collider_provider.h) |
| `cpp20-standard-concepts` | 标准库 | 标准库 Concepts | 37 | 0 | 0 | 37 | 5 | [源码](../../engine/object_query/game_object_query_service.h) |
| `cpp20-string-prefix-suffix` | 标准库 | `starts_with` / `ends_with` | 10 | 0 | 5 | 15 | 8 | [源码](../../tests/save/save_service_tests.cpp) |
| `cpp20-three-way-comparison` | 语言 | 三路比较 | 10 | 0 | 0 | 10 | 4 | [源码](../../engine/physics/collision/collision_target.h) |

### C++23

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp23-expected` | 标准库 | `std::expected` / `std::unexpected` | 990 | 0 | 16 | 1006 | 126 | [源码](../../engine/bootstrap/bootstrapper.h) |

### 常见但当前未检出

| 标准 | ID | 分类 | 特性 |
| --- | --- | --- | --- |
| C++14 | `cpp14-binary-literals` | 语言 | 二进制字面量 |
| C++14 | `cpp14-digit-separators` | 语言 | 数字分隔符 |
| C++14 | `cpp14-exchange` | 标准库 | `std::exchange` |
| C++17 | `cpp17-fallthrough` | 语言 | `[[fallthrough]]` |
| C++17 | `cpp17-maybe-unused` | 语言 | `[[maybe_unused]]` |
| C++20 | `cpp20-bit-cast` | 标准库 | `std::bit_cast` |
| C++20 | `cpp20-consteval` | 语言 | `consteval` |
| C++20 | `cpp20-constinit` | 语言 | `constinit` |
| C++20 | `cpp20-coroutines` | 语言 | 协程 |
| C++20 | `cpp20-format` | 标准库 | `std::format` |
| C++20 | `cpp20-jthread` | 标准库 | `std::jthread` / 停止令牌 |
| C++23 | `cpp23-byteswap` | 标准库 | `std::byteswap` |
| C++23 | `cpp23-if-consteval` | 语言 | `if consteval` |
| C++23 | `cpp23-mdspan` | 标准库 | `std::mdspan` |
| C++23 | `cpp23-print` | 标准库 | `std::print` / `std::println` |
| C++23 | `cpp23-to-underlying` | 标准库 | `std::to_underlying` |

<!-- END GENERATED: cpp-language-feature-stats -->

## 使用扫描器

在仓库根目录使用 PowerShell 7 或更高版本运行：

```powershell
# 输出完整 Markdown 统计区块，不修改文件
.\scripts\audit_cpp_language_features.ps1

# 重新生成本页的统计区块
.\scripts\audit_cpp_language_features.ps1 -Update

# 检查本页是否与当前源码一致
.\scripts\audit_cpp_language_features.ps1 -Check

# 列出某项特性的全部命中位置
.\scripts\audit_cpp_language_features.ps1 -Feature cpp20-span

# 验证过滤、检测与符号分类逻辑
.\scripts\audit_cpp_language_features.ps1 -SelfTest
```

`-Check` 还会验证每个已用特性都有仍然有效的代表性源码，并检查所有出现的
`std::` 标识符是否已归入现代特性注册表或明确的旧标准/通用库基线。出现新符号时，
维护者必须先判断版本归属，统计才会通过。

## 自动统计的边界

扫描器刻意不为下列语义型能力制造伪精确数字：

- 保证复制消除、隐式移动和返回值优化等由语义与编译器行为共同决定的特性。
- 聚合初始化、统一初始化和类模板实参推导中无法仅凭局部文本可靠归因的形式；
  当前只统计能够明确识别的 `std::array` CTAD。
- 编译器扩展、预处理后才出现的代码，以及第三方头文件内部使用的特性。
- 仅凭重载解析、模板实例化结果或类型系统推导才能确认的间接用法。

因此，“全面”表示完整覆盖扫描器注册并能从当前第一方源码稳定识别的 C++11–23
特性族，同时列出常见但当前未检出的候选；它不是 ISO C++ 标准条款逐项合规矩阵。

## 维护约定

新增检测项时，应在脚本注册表中给出稳定 ID、标准版本、分类、检测模式和代表性源码，
并扩充 `-SelfTest`。修改源码后若 `-Check` 报告统计过期，运行 `-Update`，审阅数字变化
及代表性链接，再提交文档。编码取舍仍以 [C++ 代码规范](coding-standard.md)为准。
