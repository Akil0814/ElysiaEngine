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

不要在 `on_enter()`、`on_exit()`、`reset()` 或渲染阶段递归请求切换；也不要在已请求致命错误后继续提交依赖损坏状态的业务操作。

## 参考

- [诊断 Payload 与辅助函数](../../../../engine/builtin/scenes/application_failure_scene_payload.h)
- [错误场景实现](../../../../engine/builtin/scenes/application_failure_scene.cpp)
- [启动加载](startup-loading.md)、[日志](../tools/utilities.md)、[返回使用指南](../README.md)
