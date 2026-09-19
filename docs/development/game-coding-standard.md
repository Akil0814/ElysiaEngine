# Elysia 游戏层代码规范

本规范适用于基于 Elysia Engine 开发的游戏项目，旨在统一代码风格、明确模块职责并降低多人协作的维护成本。

游戏层允许快速原型开发与功能迭代，但应确保代码的可读性、可维护性及基本完整性。

###

宏
ELYSIA_CONTROLLER G:\Coding\Projects\ElysiaEngine\engine\gameplay\control\controller_manager.h
ELYSIA_ANIMATIONS G:\Coding\Projects\ElysiaEngine\engine\animation\animation_service.h
ELYSIA_AUDIO G:\Coding\Projects\ElysiaEngine\engine\audio\audio_service.h
ELYSIA_CAMERA G:\Coding\Projects\ElysiaEngine\engine\camera\camera_manager.h
ELYSIA_USER_CONFIG G:\Coding\Projects\ElysiaEngine\engine\config\user_config_service.h
ELYSIA_TIME G:\Coding\Projects\ElysiaEngine\engine\core\time.h
ELYSIA_EFFECTS G:\Coding\Projects\ElysiaEngine\engine\effects\effect_service.h
ELYSIA_GAMEPLYA_COLLISION G:\Coding\Projects\ElysiaEngine\engine\gameplay\collision\gameplay_collision_service.h
ELYSIA_LOCALIZATION G:\Coding\Projects\ElysiaEngine\engine\localization\localization_service.h
ELYSIA_OBJECT_QUERY G:\Coding\Projects\ElysiaEngine\engine\object_query\game_object_query_service.h
ELYSIA_RESOURCES G:\Coding\Projects\ElysiaEngine\engine\resources\resource_service.h
ELYSIA_SAVE G:\Coding\Projects\ElysiaEngine\engine\save\save_service.h
ELYSIA_DEBUG_DRAW G:\Coding\Projects\ElysiaEngine\engine\tools\debug_draw.h


## 1. 代码命名规范

* 类、结构体、枚举使用 `PascalCase`。
* 枚举项使用 `PascalCase`。
* 私有及受保护成员变量使用 `_name_name`。
* 公开数据成员使用 `name_name`。
* 局部变量及函数参数使用 `name_name`。
* 函数、成员函数使用 `name_name`。
* 文件名及命名空间使用 `name_name`。
* 普通 `const` 变量遵循对应作用域的命名规范。
* 编译期命名常量（`constexpr`、`inline constexpr`、`static constexpr`）使用 `k_name_name`，不受成员变量命名规则限制。
* 类型别名使用 `PascalCase`。
* 宏使用 `UPPER_SNAKE_CASE`，并添加游戏项目专属前缀。
* `ELYSIA_` 前缀保留给引擎使用，游戏层不得将其用于自定义宏。

### 1.1 类与类型命名规范

* 类名应使用名词或名词短语，准确描述其职责。
* 不使用 `C`、`I`、`S` 等匈牙利式类型前缀。
* 抽象类与接口不强制添加 `Abstract`、`Base` 或 `I` 前缀。
* 只有当 `Base` 能明确表达类型职责时才使用。
* 类型缩写按普通单词处理，例如 `UiWindow`、`AiController`。
* 模板类型参数使用 `T` 或 `TName`。
* 同一概念在整个项目中应保持命名一致。
* 不得仅为满足命名规则而创建没有实际职责的类。

### 1.2 Manager

* `Manager` 用于负责对象或资源的生命周期管理、状态维护及相关模块的协调。
* `Manager` 应具有明确的管理对象和职责范围。
* `Manager` 不因被多个类使用就必须设计为单例。
* `Manager` 的生命周期与所有权应根据实际需求决定。
* 避免将无关功能集中到同一个 `Manager` 中。

### 1.3 Service

* `Service` 用于向其他游戏模块暴露特定能力，提供明确的操作接口，并隐藏内部管理机制与实现细节。
* `Service` 应具有明确的职责范围。
* 并非所有暴露能力的类都需要 `Service` 后缀，数据类型、对象模型等仍根据其实际职责命名。
* 不应仅为了满足命名规范而创建没有实际作用的转发层。

---

## 2. 生命周期命名规范

* `initialize()` / `shutdown()`：建立和销毁模块的可用状态。
* `start()` / `update()` / `reset()`：启动、推进和重置持续运行或异步流程。
* `load()`：从文件或其他外部数据源读取内容。
* `configure()`：更新配置，不负责完整模块生命周期。
* `bind()`：注入或连接运行时依赖。
* `is_initialized()`：查询模块是否已经完成初始化。

### 生命周期要求

* 函数命名应准确表达实际行为，不得仅为统一形式而改变函数语义。
* 显式生命周期管理应成对设计 `initialize()` 和 `shutdown()`。
* 采用 RAII 管理生命周期的类型不强制提供上述接口。
* 重复初始化、重复关闭及初始化失败后的行为应具有明确约定。
* 不得在对象或模块尚未具备可用状态时执行依赖其初始化结果的操作。
* 如果模块公开初始化状态，状态查询统一使用 `is_initialized()`。

---

## 3. auto 使用约定

* 默认优先显式声明类型，尤其是游戏模块公共接口及核心玩法逻辑。
* 当类型无法直观判断时，不使用 `auto` 隐藏类型信息。
* 迭代器、泛型代码、Lambda、智能指针工厂等场景允许使用 `auto`。
* 使用 `auto` 时，不应隐藏重要的指针、引用或所有权语义。
* 不应仅为了减少代码长度而使用 `auto`。
* 当使用 `auto` 会降低代码可读性时，应改为显式类型声明。

---

## 4. 代码完整性要求

### 4.1 基本要求

* 每次提交的源码必须在项目约定的构建配置下处于可编译状态。
* 不得提交存在明显语法错误或缺失必要依赖的代码。
* 未实现功能不得伪装成正常执行结果。
* 修改已有功能时，应确保相关功能及已有测试通过。
* 不得为了使代码通过编译而随意删除必要逻辑或绕过错误处理。

### 4.2 TODO 标记

* 未完成实现使用 `TODO` 标记。
* `TODO` 应明确说明尚未完成的工作。
* 不得使用 `TODO` 掩盖已知的严重错误。
* 未完成功能不得在缺乏明确处理的情况下影响正常游戏流程。
* 允许保留不影响现有功能的 `TODO`，但应在相关功能正式完成前处理。

### 4.3 TMP 标记

* 临时测试、调试及原型代码使用 `TMP` 标记。
* `TMP` 应明确标识临时代码的用途。
* 允许临时代码存在于开发分支，用于快速验证玩法及技术方案。
* 临时代码应尽可能与正式功能隔离，不得意外影响正常游戏流程。
* 临时代码不得破坏已有功能或引入无法控制的副作用。
* 原型功能被正式采用后，应移除 `TMP` 标记并整理为正式实现。
* 正式发布前必须移除或禁用不再需要的临时代码。

### 4.4 调试与测试

* 临时调试逻辑应使用 `TMP` 标记。
* 正式的日志、诊断和测试代码不属于临时代码，不需要使用 `TMP` 标记。
* 不得将临时调试输出作为正式错误处理机制。
* 性能测试及压力测试代码应避免意外进入正常游戏流程。

---

## 5. 游戏层与引擎层边界

* 游戏层应优先使用 Elysia Engine 提供的公共接口。
* 不应直接依赖引擎内部的 `Manager` 或其他非公开实现。
* 不应为了实现游戏功能而绕过引擎已有的资源、场景、输入或物理管理机制。
* 游戏层负责具体玩法、角色行为、游戏规则及内容组织。
* 通用且与具体游戏无关的底层能力应优先考虑由引擎层提供。
* 不得仅因为某个功能可能被其他游戏复用，就强制将其移动到引擎层。
* 修改引擎公共接口时，应考虑对现有游戏模块的影响。

---

## 6. 规范执行原则

* 代码可读性与行为正确性优先于机械地遵守命名形式。
* 不应为了满足规范而引入不必要的抽象、包装或复杂度。
* 对于确有必要的特殊实现，允许在保持代码清晰的前提下合理例外。
* 新增代码应遵守本规范。
* 修改已有代码时，应优先保证功能正确，不强制进行无关的大规模格式重构。
* 规范发生调整时，以项目仓库中的最新版本为准。
