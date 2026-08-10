# Moonline：C++11 及以上特性审查

> 审查日期：2026-07-13  
> 审查范围：`engine/`、`game/`、`tests/` 与 `main.cpp` 中的 `.cpp` / `.h` 文件；不含 `thirdparty/`、`build/`、`out/` 和二进制产物。
> 审查方式：静态检索结合代表性源码阅读。本文只记录在项目自有代码中能确认的实际使用，不将 `CMAKE_CXX_STANDARD 23` 视为“使用了全部 C++23 特性”。

## 结论概览

项目构建目标为 **C++23**（根目录 `CMakeLists.txt:8`）。实际代码覆盖了 C++11、C++14、C++17、C++20 和 C++23 的一批核心语言与标准库特性：

| 标准版本 | 覆盖情况 | 代表性能力 |
| --- | --- | --- |
| C++11 | 广泛使用 | RAII、智能指针、移动语义、lambda、并发、变参模板、`override`/`final` |
| C++14 | 已使用 | `std::make_unique`、lambda 初始化捕获、泛型 lambda |
| C++17 | 广泛使用 | `optional`、`variant`、`any`、`filesystem`、`string_view`、折叠表达式、`if constexpr` |
| C++20 | 已使用 | `source_location`、`span`、指定成员初始化、默认比较、嵌套命名空间定义 |
| C++23 | 已使用 | `std::expected` / `std::unexpected` 的显式错误处理 |

按本次检索，项目**未发现**协程、Concepts/`requires`、Ranges、Modules、`std::format`、结构化绑定、`consteval` / `constinit` 的实际使用。它们不是项目质量的必备条件，不应在简历或项目介绍中声称已经采用。

## C++11

### RAII、智能指针与移动语义

项目以 `std::unique_ptr` 表达 UI 树、场景对象和资源的唯一所有权，以 `std::shared_ptr` 管理可共享生命周期对象；通过 `std::move` 进行所有权转移。

- `engine/ui/core/ui_child_host.h:41-60`：`ChildEntry` 持有 `std::unique_ptr<UiElement>`，显式删除复制操作并定义 `noexcept` 移动操作，避免 UI 子节点被意外复制。
- `engine/ui/core/ui_child_host.h:94-116`：模板工厂使用 `std::make_unique`、`std::forward` 创建并交由容器接管。
- `engine/scene/scene.h:68-76`：通用对象创建接口以完美转发创建对象。
- `engine/testbed/scene/ui_test_scene.cpp:63-104`：大量用 `std::make_unique` 构建嵌套 UI 组件树。

这里的所有权模型是项目现代 C++ 实践中最重要的一部分：原始指针仅作为非拥有访问句柄使用，容器承担对象释放责任。

### lambda 表达式、捕获与回调

- `engine/testbed/scene/ui_test_scene.cpp:176-267`：`[this]` 回调用于 UI 的点击、选择和数值改变事件。
- `tests/termination_manager_tests.cpp:77`：lambda 作为线程入口。
- `engine/ui/widgets/ui_button.cpp:324-333`：回调组合逻辑使用 lambda 封装原有和新增动作。

### 语言基础：类型推导、范围 for、强类型枚举、空指针与虚函数控制

- `auto` 与范围 for：例如 `engine/gameplay/scene/gameplay_scene.cpp`、`engine/core/render/sdl_render_command_executor.h:256`。
- `enum class`：例如 `engine/ui/core/ui_child_host.h:21` 的 `UiChildStyleRelation`，防止枚举值隐式转换。
- `nullptr`、`override`、`final`、`noexcept`：遍布引擎接口和测试桩；例如 `tests/ui/ui_focus_routing_tests.cpp:49-56`。
- `= delete` / `= default`：例如 `engine/ui/core/ui_child_host.h:49-59`，明确对象可复制/可移动语义。
- `constexpr` 与 `static_assert`：例如 `engine/core/geometry/vector2.h` 的编译期几何值，以及 `engine/ui/core/ui_child_host.h:102` 对子类型的约束。

### 变参模板与完美转发

- `engine/scene/scene.h:68-76`：`template <typename T, typename... Args>` 与 `Args&&...`。
- `engine/ui/core/ui_child_host.h:94-116`：变参模板结合 `std::forward` 实现类型安全的 UI 子节点创建。

### 并发基础库

- `tests/termination_manager_tests.cpp:76-92`：使用 `std::atomic<bool>` 和 `std::thread` 验证终止请求发布时的并发行为。

这证明项目至少针对一个共享状态场景进行了 C++ 线程与原子可见性的测试；它不等价于整个运行时是多线程架构。

## C++14

### `std::make_unique`

`std::make_unique` 在 UI、场景和测试中广泛使用，例如 `engine/ui/core/ui_child_host.h:104` 与 `engine/testbed/scene/ui_test_scene.cpp:69`。它避免了直接 `new` 带来的异常安全和所有权表达问题。

### lambda 初始化捕获

- `game/scene/character_select_scene.cpp:153`：`[this, selected_key = character_key]` 将选中的角色键复制到回调中。
- `engine/testbed/scene/ui_test_scene.cpp:251`：`[this, theme = themes[index]]` 捕获循环当前主题值，避免悬空或错误引用循环变量。
- `engine/ui/widgets/ui_button.cpp:329`：`[before = std::move(on_click), after = std::move(existing)]` 以移动方式组合回调。

### 泛型 lambda

- `engine/ui/composites/ui_tab_container.cpp:265-266`：`[this](auto index)` 用同一套逻辑接收不同回调来源的索引参数。

## C++17

### `std::optional`、`std::variant` 与 `std::any`

- `std::optional`：在 70 个自有源码文件中出现；例如 `engine/camera/camera_controller.h:18-47` 用可选矩形表达可缺省的相机焦点和世界边界。
- `std::variant`：`engine/ui/widgets/ui_button.h:43-48` 使用 `UiButtonContent` 表示按钮可承载的多种内容；`engine/loading/game_content_loader.h:51-54` 为预加载任务建立类型安全的联合数据。
- `std::any`：`engine/scene/routing/scene_payload.h` 定义场景负载，`game/scene/main_menu_scene.cpp:25` 通过指针版 `std::any_cast` 做无异常类型检查。

### `std::string_view` 与 `std::filesystem`

- `std::string_view`：在配置与资源模块中用于非拥有的语言和配置 key 参数，减少不必要的复制。
- `std::filesystem`：例如 `engine/config/user/user_config_store.cpp` 处理设置文件，测试中用其创建和清理临时资源目录。

### 编译期分支与折叠表达式

- `engine/resources/atlas/atlas.h:41-51`：使用 `if constexpr` 检查空参数包，并使用一元折叠表达式验证全部指针非空、顺序添加全部纹理。
- `engine/ui/core/ui_child_host.h:94-101`：`sizeof...(Args)`、`std::tuple_element_t`、`std::decay_t`、`std::enable_if_t` 组合，阻止容易误用的重载匹配。

### if 初始化语句、内联变量、标准属性和算法

- if 初始化语句：`engine/config/user_config.cpp` 使用 `if (const auto handler = ...; !handler)` 将结果作用域限制在判断语句中。
- 内联变量：`engine/scene/routing/scene_key.h` 的 engine 内建键与 gameplay 项目键可安全放在头文件中。
- `[[nodiscard]]`：大量用于查询/计算接口，例如 `engine/core/time.h:18-23`，降低忽略返回值的风险。
- `std::clamp`：例如 `engine/input/translator/gamepad_input_translator.cpp:243` 对手柄轴值做范围限制。

### 嵌套命名空间定义

代码广泛采用 `namespace elysia::... {}` 这一 C++17 简写形式，例如 `engine/resources/atlas/atlas.h:10`。

## C++20

### `std::source_location`

- `engine/tools/logger.h:53-68`：日志 API 的默认参数为 `std::source_location::current()`，调用点无需手写文件名和行号。
- `engine/application/application.h` 与 engine 内建 StartupLoadingScene：启动失败与内容加载失败会携带调用位置。

它比 `__FILE__` / `__LINE__` 宏更适合封装成函数默认参数，也利于日志接口的复用。

### `std::span`

- `engine/physics/collider_provider.h:14`：`std::span<const Collider>` 向调用方暴露连续碰撞体视图，而不转移所有权、不复制容器。

### 指定成员初始化（designated initializers）

- `engine/testbed/scene/ui_test_scene.cpp:53-58`：按字段名构造 `UiLayoutChildOptions`。
- `engine/testbed/scene/ui_test_scene.cpp:241`：按字段名构造 `UiOverlayOptions`。

这提高了配置型结构体初始化的可读性，并降低成员顺序变更导致的误填风险。

### 默认比较运算符

- `engine/audio/audio_settings.h:11`：`friend bool operator==(const AudioSettings&, const AudioSettings&) = default;`。
- `engine/config/user_config_data.h:18`：用户窗口尺寸与设置使用默认相等比较。
- `engine/core/render/color.h:19`：颜色值使用默认 `operator==`。

## C++23

### `std::expected` / `std::unexpected`

项目的设置与初始化路径使用值语义错误处理，而不是用异常承载可预期业务失败。

- `engine/config/user/user_config_store.h`：用户设置加载/保存返回 `std::expected`。
- `engine/config/user_config.cpp`：设置校验或运行时应用失败时返回 `std::unexpected<UserConfigFailure>`；调用端通过 `if (...; !result)` 传播错误。
- `engine/config/config_service.cpp:5-24`：配置服务初始化和保存将底层错误逐层封装/返回。
- `engine/application/application.h/.cpp`：engine 应用层实现运行时设置变更，并向配置层返回可诊断失败。

这是当前代码中最明确的 C++23 标准库特性使用点，适合在项目介绍中单独说明。

## 未发现或未纳入“已使用”表述的特性

以下项目经静态检索未发现实际使用，或不应仅凭构建标准就计为已使用：

| 标准 | 未发现的代表性特性 |
| --- | --- |
| C++17 | 结构化绑定、`std::execution` 并行算法 |
| C++20 | Concepts / `requires`、协程、Ranges、Modules、`std::format`、`consteval`、`constinit` |
| C++23 | `std::print`、`std::generator`、`std::mdspan`、显式对象参数（deducing `this`） |

此外，`std::thread` 与 `std::atomic` 的使用当前主要出现在终止管理器测试中；在没有进一步架构证据的情况下，项目不宜描述为“多线程游戏引擎”。

## 对外介绍建议

可准确地将项目表述为：

> 基于 C++23、SDL2 与 CMake 的 2D 游戏项目。工程中采用 RAII 与智能指针管理 UI/场景对象生命周期，使用 C++17 的 `optional`、`variant`、`filesystem` 构建配置与资源流程，使用 C++20 `source_location` 实现调用点日志，并以 C++23 `std::expected` 建模可预期的配置与运行时设置错误。

避免写成“覆盖/掌握全部 C++23 特性”或“多线程引擎”；前者不符合代码证据，后者会高估现有并发范围。
