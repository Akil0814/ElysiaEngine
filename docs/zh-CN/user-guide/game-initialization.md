# 游戏项目初始化指南

本指南介绍如何通过 Elysia Engine 初始化自己的游戏项目，包括程序入口、游戏模块定义、应用配置及场景注册。

本文以仓库中的示例游戏为参考。

## 1. 初始化流程

Elysia Engine 通过游戏模块 `IGameModule` 获取游戏配置，并完成应用初始化。

基本流程如下：

```text
main()
  |
  v
创建 GameModule
  |
  v
ELYSIA_INITIALIZE_APP
  |
  +-- 获取 ApplicationDescriptor
  |
  +-- 注册游戏场景
  |
  v
ELYSIA_RUN_APP
  |
  v
返回退出状态
```

游戏开发者通常只需要提供自己的游戏模块及游戏内容，不需要在程序入口中手动初始化各个引擎子系统。

## 2. 程序入口

仓库中的 [main.cpp](../../../main.cpp) 展示了应用的基本启动方式。

```cpp
#include <SDL3/SDL_main.h>
#include "engine/application/application.h"
#include "game/application/example_game_module.h"

#include <cstdlib>

int main(int argc, char** argv)
{
    example::application::GameModule game_module;

    if (!ELYSIA_INITIALIZE_APP(argc, argv, game_module))
        return EXIT_FAILURE;

    return ELYSIA_RUN_APP ==
                   elysia::application::ApplicationRunResult::NormalExit
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
```

程序入口主要完成以下工作：

1. 创建游戏模块。
2. 调用 `ELYSIA_INITIALIZE_APP` 初始化应用。
3. 初始化失败时返回 `EXIT_FAILURE`。
4. 调用 `ELYSIA_RUN_APP` 运行应用。
5. 根据运行结果返回程序退出状态。

开发者可以将示例中的 `GameModule` 替换为自己的游戏模块。

## 3. 定义游戏模块

游戏模块通过继承 `elysia::application::IGameModule` 向引擎提供游戏专属的配置和内容。

接口定义位于：

[game_module.h](../../../engine/application/game_module.h)

### 3.1 IGameModule 接口

```cpp
class IGameModule
{
public:
    virtual ~IGameModule() = default;

    [[nodiscard]]
    virtual ApplicationDescriptor descriptor() const = 0;

    virtual void register_scenes(
        elysia::scene::SceneManager& scene_manager) const = 0;

    [[nodiscard]]
    virtual std::unique_ptr<elysia::tools::IDevelopmentOverlay>
        create_development_overlay() const
    {
        return {};
    }
};
```

接口包含三个方法：

| 方法                             | 用途            | 是否必须实现 |
| ------------------------------ | ------------- | ------ |
| `descriptor()`                 | 提供应用配置与初始场景路由 | 是      |
| `register_scenes()`            | 注册游戏场景        | 是      |
| `create_development_overlay()` | 创建自定义开发面板     | 否      |

`create_development_overlay()` 默认返回空指针。不需要自定义开发面板时，可以不重写该方法。

### 3.2 创建自己的 GameModule

参考示例游戏，可以创建一个继承 `IGameModule` 的类。

```cpp
#pragma once

#include "../../engine/application/game_module.h"

namespace example::application
{

class GameModule final
    : public elysia::application::IGameModule
{
public:
    [[nodiscard]]
    elysia::application::ApplicationDescriptor
        descriptor() const override;

    void register_scenes(
        elysia::scene::SceneManager& scene_manager
    ) const override;
};

}
```

以上是仅包含必需接口的版本。

如果需要开发面板，可以额外重写 `create_development_overlay()`。

完整示例：

* [example_game_module.h](../../../game/application/example_game_module.h)
* [example_game_module.cpp](../../../game/application/example_game_module.cpp)

## 4. 配置应用

通过 `descriptor()` 返回 `ApplicationDescriptor`，向引擎提供游戏的基础配置。

```cpp
struct ApplicationDescriptor
{
    int logical_width = 1280;
    int logical_height = 720;

    elysia::scene::SceneRoute initial_route{};

    ApplicationPresentationSettings presentation{};
};
```

主要配置包括：

| 字段               | 用途           |
| ---------------- | ------------ |
| `logical_width`  | 逻辑画面宽度       |
| `logical_height` | 逻辑画面高度       |
| `initial_route`  | 应用启动时的初始场景路由 |
| `presentation`   | 应用的显示与呈现配置   |

例如，可以在 `descriptor()` 中设置逻辑分辨率：

```cpp
elysia::application::ApplicationDescriptor
GameModule::descriptor() const
{
    elysia::application::ApplicationDescriptor descriptor;

    descriptor.logical_width = 1280;
    descriptor.logical_height = 720;

    // 配置初始场景路由。

    return descriptor;
}
```

注意：以上代码仅展示部分配置。实际项目还需要提供有效的初始场景路由。

### 4.1 配置初始场景

应用通过 `initial_route` 指定启动后进入的场景。

示例项目使用引擎内置的 `StartupLoading` 场景，并通过 `StartupLoadingScenePayload` 指定加载成功后的目标场景。

```text
Application
    |
    v
StartupLoading
    |
    | 加载成功
    v
MainMenu
```

开发者可以通过初始场景路由配置启动流程。

具体配置及使用方式请参阅：

[启动加载场景](builtin-scenes/startup-loading.md)

## 5. 注册游戏场景

游戏模块通过 `register_scenes()` 向 `SceneManager` 注册游戏场景。

例如，注册一个主菜单场景：

```cpp
void GameModule::register_scenes(
    elysia::scene::SceneManager& scene_manager) const
{
    scene_manager.register_game_scene<
        example::scene::MainMenuScene>(
            example::scene_keys::MainMenu);
}
```

实际项目可以根据需要注册多个场景。

场景注册时，应确保：

* 场景类型及对应的 `SceneKey` 正确。
* 初始路由所引用的游戏场景已注册。
* 后续场景切换使用有效的场景标识符。

有关场景职责、生命周期与销毁时机，请阅读[核心概念](core_concepts.md)。

## 6. 自定义开发面板（可选）

`IGameModule` 提供可选的 `create_development_overlay()` 接口。

示例项目使用 `ImGuiDevelopmentOverlay`：

```cpp
std::unique_ptr<elysia::tools::IDevelopmentOverlay>
GameModule::create_development_overlay() const
{
#if ELYSIA_ENABLE_IMGUI
    return std::make_unique<
        elysia::tools::ImGuiDevelopmentOverlay>();
#else
    return {};
#endif
}
```

使用该实现时，需要引入相应的开发面板头文件，并确保构建配置与代码中的宏保持一致。

如果项目不需要自定义开发面板，可以不重写此方法。

## 7. 完整参考实现

仓库中的示例游戏提供了完整的初始化与集成实现。

| 文件                                                                           | 内容               |
| ---------------------------------------------------------------------------- | ---------------- |
| [main.cpp](../../../main.cpp)                                                | 程序入口与应用运行        |
| [game_module.h](../../../engine/application/game_module.h)                   | 游戏模块接口定义         |
| [example_game_module.h](../../../game/application/example_game_module.h)     | 示例游戏模块声明         |
| [example_game_module.cpp](../../../game/application/example_game_module.cpp) | 应用配置、场景注册及开发面板实现 |

完成游戏模块接入后，可以继续阅读：

[GameplayScene 使用指南](gameplay/scene.md)

了解如何组织具体的游戏玩法场景。
