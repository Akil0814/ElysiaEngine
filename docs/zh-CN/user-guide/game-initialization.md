# 游戏项目初始化指南

本文面向准备接入游戏代码的开发者，说明程序入口、游戏模块、应用配置和场景注册各自负责什么，以及如何把它们连接起来。阅读前应已能按[构建与运行指南](build-and-run.md)构建仓库。

程序入口把运行交给引擎，游戏模块提供配置并注册游戏场景，引擎负责初始化子系统和驱动场景。下面先介绍这一分工，再给出最小模块和启动加载路由示例。示例沿用仓库的 CMake 包含路径配置；模块代码可编译，但运行仍需要有效的项目配置与资源目录。

## 1. 初始化流程

引擎通过 `IGameModule` 获取应用描述并注册游戏场景。`ELYSIA_INITIALIZE_APP` 不只是保存描述：它还准备运行环境并启动初始场景。成功后，`ELYSIA_RUN_APP` 才开始持续处理输入、更新和绘制。资源中的只读游戏配置另由内容加载流程发布，见[配置指南](content/configuration.md)。

```text
main()
  |
  +-- 创建 GameModule
  |
  +-- ELYSIA_INITIALIZE_APP
  |     +-- 获取并校验 ApplicationDescriptor
  |     +-- 读取运行配置，初始化窗口、渲染器等子系统
  |     +-- 准备内置资源、输入和场景运行上下文
  |     +-- 注册内置场景，再调用 register_scenes() 注册游戏场景
  |     +-- 按 initial_route 启动初始场景
  |
  +-- 初始化成功后调用 ELYSIA_RUN_APP
  |     +-- 输入、更新、绘制
  |     +-- 退出时清理运行资源
  |
  +-- 返回程序退出状态
```

这个流程只列出接入游戏时需要理解的阶段，并非应用内部所有初始化步骤。游戏代码不需要在 `main()` 中逐个初始化引擎子系统。

## 2. 程序入口

下面保留仓库 [main.cpp](../../../main.cpp) 的启动方式和最外层异常处理。它使用现有示例游戏模块，可以作为替换游戏入口时的参考。

```cpp
#include <SDL3/SDL_main.h>
#include "engine/application/application.h"
#include "game/application/example_game_module.h"

#include <cstdlib>

int main(int argc, char** argv)
{
    try
    {
        example::application::GameModule game_module;
        if (!ELYSIA_INITIALIZE_APP(argc, argv, game_module))
            return EXIT_FAILURE;

        return ELYSIA_RUN_APP ==
                       elysia::application::ApplicationRunResult::NormalExit
                   ? EXIT_SUCCESS
                   : EXIT_FAILURE;
    }
    catch (...)
    {
        return EXIT_FAILURE;
    }
}
```

初始化返回 `false` 时，不应继续调用 `ELYSIA_RUN_APP`。运行结果为 `NormalExit` 时返回成功，其余结果返回失败。

引擎会在初始化和主要运行阶段捕获异常，并按对应边界处理失败。入口中的 `catch (...)` 则负责仍然传播到这里的 C++ 异常，例如游戏模块构造期间的异常；它只把程序退出码设为失败，不代表游戏恢复运行，也不会在这里补记日志。异常若试图逃出 `noexcept` 函数，会触发 `std::terminate()`，不能依靠这一层捕获继续清理。

## 3. 定义游戏模块

游戏模块继承 `elysia::application::IGameModule`，向引擎提供游戏专属配置和场景。接口定义见 [game_module.h](../../../engine/application/game_module.h)。

### 3.1 IGameModule 接口

| 方法 | 职责 | 是否必须实现 |
| --- | --- | --- |
| `descriptor() const` | 返回 `ApplicationDescriptor`，包含逻辑分辨率、呈现配置和初始路由 | 是 |
| `register_scenes(SceneManager&) const` | 向场景管理器注册游戏场景 | 是 |
| `create_development_overlay() const` | 返回拥有所有权的 `std::unique_ptr<IDevelopmentOverlay>`，供引擎接管开发工具覆盖层 | 否 |

`descriptor()` 提供数据，`register_scenes()` 建立场景标识符与场景类型的对应关系。注册场景不会在此处直接构造所有游戏场景；场景管理器在需要进入时创建实例。开发工具接口默认返回空指针，不使用覆盖层时无需重写。

### 3.2 创建自己的 GameModule

下面是完整的模块示例，可放入自己的头文件，例如 `game/application/my_game_module.h`。它注册一个空场景，并直接将该场景设为初始目标，用来说明配置与注册必须指向同一个有效标识符。

```cpp
#pragma once

#include "engine/application/game_module.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"

namespace initialization_example
{
// 示例选择的游戏场景键，须与本项目其他游戏场景保持唯一。
inline constexpr elysia::scene::SceneKey kEntry = 1;

class EntryScene final : public elysia::scene::Scene
{
protected:
    // 最小入口场景不持有业务对象，三个生命周期钩子在此为空实现。
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }
};

class GameModule final : public elysia::application::IGameModule
{
public:
    [[nodiscard]] elysia::application::ApplicationDescriptor
        descriptor() const override
    {
        elysia::application::ApplicationDescriptor descriptor;
        descriptor.logical_width = 1280;
        descriptor.logical_height = 720;
        // 场景注册完成后，应用通过该路由激活入口。
        descriptor.initial_route = {.target = kEntry};
        return descriptor;
    }

    void register_scenes(
        elysia::scene::SceneManager& scene_manager) const override
    {
        scene_manager.register_game_scene<EntryScene>(kEntry);
    }
};
}
```

使用这一模块时，将入口中的模块头文件和类型分别替换为自己的头文件和 `initialization_example::GameModule`。仓库的 `engine_lib`、`game_lib` 已把项目根目录加入包含路径，因此这里使用 `engine/...`，不依赖头文件恰好位于某个目录层级。

`EntryScene` 没有添加可见内容，也没有执行项目内容加载。直接进入这种场景适合验证模块接线；若场景依赖项目资源和项目字体，应按下一节接入启动加载流程。场景回调的职责与重置行为见[核心概念](core_concepts.md)和[场景系统](scene/scene.md)。

## 4. 配置应用

`descriptor()` 返回的 `ApplicationDescriptor` 决定应用启动时使用的配置。

| 字段 | 默认值及用途 |
| --- | --- |
| `logical_width` | 默认 `1280`，逻辑画面宽度；必须大于零 |
| `logical_height` | 默认 `720`，逻辑画面高度；必须大于零 |
| `initial_route` | 默认目标为 `SceneKeys::Invalid`，必须显式配置有效初始路由 |
| `presentation` | 包含纹理过滤方式、字体来源与字号、默认 UI 主题及启动引擎 Logo 选择 |

逻辑分辨率用于场景坐标和逻辑绘制，不等于窗口的实际像素尺寸。窗口设置与用户配置的关系见[配置指南](content/configuration.md)。呈现配置的完整字段与默认值见 [application_presentation_settings.h](../../../engine/application/application_presentation_settings.h)。

### 4.1 配置初始场景

上一节直接进入空场景。仓库示例则通过内置 `StartupLoading` 准备项目内容，再转到主菜单。下面的完整辅助函数创建这种路由；传入的成功路由必须指向已经注册的游戏场景。

```cpp
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/startup_loading_scene_payload.h"

#include <utility>

namespace initialization_example
{
elysia::scene::SceneRoute make_startup_route(
    elysia::scene::SceneRoute success_route)
{
    elysia::builtin::StartupLoadingScenePayload payload;
    // 正式内容准备完毕后才进入使用项目资源的目标场景。
    payload.success_route = std::move(success_route);
    payload.wait_for_logo_sequence = false;
    payload.wait_for_confirmation = true;
    return {
        .target = elysia::builtin::SceneKeys::StartupLoading,
        .payload = std::move(payload),
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}
}
```

将辅助函数放在模块使用它的位置之前，或在头文件中声明后于实现文件中定义。随后把模块中的初始路由赋值改为 `descriptor.initial_route = make_startup_route({ .target = kEntry });`，即可在内容加载和确认完成后进入 `EntryScene`。

上面两项等待配置与仓库示例一致：资源加载成功后不必等 Logo 序列结束，但仍需用户确认。因此，示例游戏的流程是：

```text
StartupLoading
    |
    +-- 加载项目内容并激活项目字体
    |
    +-- 加载成功后显示确认提示
    |
    +-- 用户确认
    v
MainMenu
```

如果希望加载完成后自动进入目标，可把 `wait_for_confirmation` 设为 `false`。两项等待配置只控制成功流程的等待条件，不会跳过内容加载或失败处理。路由 Payload、Logo 和加载失败行为见[启动加载场景](builtin-scenes/startup-loading.md)；资源目录与清单的准备见[资源加载](content/resources.md)。

## 5. 注册游戏场景

`register_scenes()` 通过 `register_game_scene<T>(key)` 注册游戏场景。前面的模块示例使用 `kEntry`；实际游戏可以继续注册主菜单、关卡和结算等场景，并在路由中使用同一组标识符。

游戏场景的 `SceneKey` 范围为 `1` 到 `999`，同一标识符不能重复注册。引擎已在调用游戏模块前注册内置场景，游戏模块不需要重复注册 `StartupLoading`。

初始目标和后续切换目标都必须已注册。使用启动加载场景时，既要让 `initial_route` 指向内置加载场景，也要在游戏模块中注册其 `success_route` 指向的游戏场景。合法的数字不代表场景已经存在。

场景构造参数、切换、传参和生命周期见[场景系统](scene/scene.md)。游戏模块只负责注册；具体对象的创建和场景内容组织应放在场景中。

## 6. 自定义开发面板（可选）

开发工具覆盖层负责工具界面的初始化、事件处理和绘制；具体开发面板则通过它提供的 `IDevelopmentPanelRegistry::register_panel()` 注册。这是两个层次：创建覆盖层不等于已经添加自定义面板。

仓库示例重写 `create_development_overlay()`，返回 `ImGuiDevelopmentOverlay`。下面是该方法的局部实现片段，需配合模块类中的同名 `const override` 声明；启用 ImGui 的分支还需包含 `engine/tools/imgui/imgui_development_overlay.h`。

```cpp
std::unique_ptr<elysia::tools::IDevelopmentOverlay>
GameModule::create_development_overlay() const
{
#if ELYSIA_ENABLE_IMGUI
    return std::make_unique<elysia::tools::ImGuiDevelopmentOverlay>();
#else
    return {};
#endif
}
```

只有 CMake 选项 `ELYSIA_ENABLE_IMGUI` 启用时，应用才调用这个工厂并接管返回的覆盖层。关闭该选项时，即使模块重写了方法，应用也不会调用它。无需开发工具时，可以保留接口的默认空实现。

覆盖层由引擎初始化、驱动和关闭，游戏模块不应自行调用其运行周期方法。自定义实现还必须遵守 `shutdown() noexcept` 等接口约定，不能让异常逃出这些函数。完整接口见 [development_overlay.h](../../../engine/tools/development_overlay.h)，可工作的工厂声明与实现见仓库游戏模块。

注册与注销面板的完整场景示例见[开发面板指南](tools/development-panels.md)。

## 7. 完整参考实现

本文给出的空场景模块用于说明最小接线，不包含仓库示例游戏的全部配置。需要核对实际启动方式时，可从以下文件继续阅读：

| 文件 | 内容 |
| --- | --- |
| [main.cpp](../../../main.cpp) | 程序入口、运行结果与异常兜底 |
| [game_module.h](../../../engine/application/game_module.h) | 模块接口和应用描述结构 |
| [example_game_module.h](../../../game/application/example_game_module.h) | 示例模块的三个接口声明 |
| [example_game_module.cpp](../../../game/application/example_game_module.cpp) | 呈现配置、加载路由、场景注册和覆盖层工厂 |

完成模块接入后，可继续阅读 [GameplayScene 使用指南](gameplay/scene.md)，了解如何组织具体玩法场景。
