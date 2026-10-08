# 应用错误场景

`ApplicationFailure` 用于展示启动资源失败或运行时致命错误的诊断，并提供退出流程。它不是通用重试、回档或恢复游戏的机制；可恢复的业务失败应由游戏自己的流程处理。

## 在场景中请求错误展示

以下方法体放在派生 `Scene` 中，在允许发出路由请求的输入或更新阶段调用。引擎及内置 UI 资源必须已经可用。

```cpp
#include "engine/builtin/scenes/application_failure_scene_payload.h"

// 派生 Scene 的成员方法：
void show_fatal_error()
{
    request_scene_switch(elysia::builtin::make_application_failure_route(
        elysia::builtin::ApplicationFailurePresentation::RuntimeFatal,
        "gameplay",
        "Required game state could not be restored."));
}
```

通过 `ContentLoadFailure` 的辅助函数重载可生成启动加载失败路由，保留错误类别、代码和诊断。不要直接传字符串代替 `ApplicationFailureScenePayload`。

## 诊断内容

Payload 包含 `presentation`、`reason`、`error_code`、`category` 和 `diagnostic`。`StartupLoading` 与 `RuntimeFatal` 是展示模式，决定对应的文案和表现，不表示任意两种错误都能够恢复。

场景进入时检查 Payload 类型及展示模式。它会停用项目字体，使用内置展示资源降低对已失败项目内容的依赖。诊断仍应提供可追踪的类别与原因，不能仅展示“失败了”而丢失定位信息。

## 退出与能力边界

用户确认退出会向终止管理器提交终止请求，由应用统一处理退出，不是返回之前的场景。场景自身没有成功返回路由，也没有通用“重新加载并继续游戏”的按钮。

只有渲染、窗口、必要内置资源和错误 UI 仍可工作时才能展示该界面。过早的初始化失败、错误界面构建失败或无法继续安全渲染的故障不能靠路由解决。也不要假定任意异常都会自动转成这个场景；应遵循应用已有错误边界，保留日志并处理最终退出结果。

不要在 `on_enter()`、`on_exit()`、`on_reset()` 或渲染阶段递归请求切换；也不要在已请求致命错误后继续提交依赖损坏状态的业务操作。

场景逻辑异常的恢复顺序是：解除并退出当前场景，销毁故障场景及其对象，清理成功后才调用失败路由工厂并构造恢复场景。清理中某一步抛异常仍会继续其余清理，最终进入 Faulted，不调用失败路由工厂；恢复场景失败也不会递归恢复。`on_enter` 抛异常不会补调 `on_exit`，游戏侧外部副作用须由 RAII 或局部回滚撤销。

## 参考

- [诊断 Payload 与辅助函数](../../../../engine/builtin/scenes/application_failure_scene_payload.h)
- [错误场景实现](../../../../engine/builtin/scenes/application_failure_scene.cpp)
- [启动加载](startup-loading.md)、[日志](../tools/utilities.md)、[返回使用指南](../README.md)

## 诊断传播与底层渲染失败

`SceneBoundaryFailure` 用类型化字段保存场景键和边界，用 `FailureDiagnostic` 保存消息、来源及额外条目。日志和失败路由统一通过 `to_failure_diagnostic()` 投影主场景上下文，不修改原始失败。通过 `make_application_failure_route(failure)` 转换时完整保留诊断；通用失败路由的第三个参数为 `FailureDiagnostic`，消息需要在调用点显式通过 `make_failure_diagnostic()` 创建。

底层 SDL 渲染失败使用 `RenderFailure` 和 `RenderBackendError`，直接进入应用故障退出，不进入错误场景。执行器返回 `expected`，先恢复已修改的状态，保留恢复错误，然后停止后续绘制；应用跳过提交和帧等待并执行完整关闭。关闭失败同样返回 `FaultExit`。

确认退出前写入完整诊断日志；有界终止记录保留诊断来源，并在截断时指引查看原始日志。Release 界面仍遵循隐藏原始诊断的现有呈现规则。

渲染执行器的单条和批量 `execute_render_command(s)` 返回 `[[nodiscard]] expected<void, RenderFailure>`，批量在首个失败后停止。`RenderFailure` 保存操作名及统一诊断；Scene 使用 `RenderBackendError` 传播到应用边界。执行器在绘制失败后仍尝试恢复已读取且已修改的状态，恢复错误附加到首个失败。清屏与帧提交走同一故障退出路径，失败后不再提交后续帧或等待帧节奏。

场景清理期间也遵循后端错误优先规则：完成必要回收后，传播首个 `RenderBackendError`，将原始普通异常和后续清理失败保留为诊断上下文。恢复场景及缓存场景销毁同样适用；最终关闭边界只记录并返回关闭结果，不再次抛出。
