# 设置场景

内置 `Settings` 场景提供窗口、帧率、垂直同步、音量和语言设置。应用需要已经初始化 `UserConfigService`，场景必须收到有效的 `SettingsScenePayload`。它是切换到的独立场景，不是自动叠加在当前游戏画面上的暂停面板。

## 从游戏场景打开

在派生 `Scene` 中定义如下方法，并从按钮回调或正常输入、更新阶段调用。`return_route` 指向已经注册的返回场景，必要时携带该场景自己的 Payload。

```cpp
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/settings_scene_payload.h"
#include <utility>

// 派生 Scene 的成员方法体：
void open_settings(elysia::scene::SceneRoute return_route)
{
    elysia::builtin::SettingsScenePayload payload;
    payload.return_route = std::move(return_route);
    payload.visibility.target_fps = false;
    request_scene_switch(elysia::scene::SceneRoute{
        .target = elysia::builtin::SceneKeys::Settings,
        .payload = std::move(payload),
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    });
}
```

`request_scene_switch` 是场景的受保护接口，上述代码不是独立自由函数。不要在 `on_enter()`、`on_exit()` 或渲染回调中递归请求场景切换。缺少正确 Payload、返回路由无效或用户配置服务未初始化，都会导致接入错误。

## 可见项与保存

`visibility` 的 `window_mode`、`target_fps`、`vsync`、`master_volume`、`music_volume`、`sound_volume`、`language` 默认均为 `true`。它们控制界面提供哪些项目，不是新增应用能力，也不是修改配置默认值的接口。

面板维护草稿；改变控件不会自动等同于已保存。点击保存时，场景从当前配置快照组装请求，并调用 `apply_and_save_user_config` 应用及持久化。

- 成功：刷新运行状态基线与面板草稿，显示已保存或需要重启的提示；不会自动返回游戏。
- 失败：显示失败原因。服务可能执行回滚；回滚失败时也会显示相应信息。场景以服务返回后的实际运行快照刷新草稿，不能假定所有设置都维持请求值或全部恢复旧值。
- 返回或取消：放弃尚未保存的草稿，使用 `return_route` 离开；不会撤销此前已经成功保存的设置。

语言候选来自本地化服务支持列表，窗口尺寸候选参考显示器可用区域。不要把隐藏某项理解为删除该设置，也不要假定所有设置都能立即无条件生效。

## 返回与生命周期

返回路由的 Reuse/Reset/Recreate 决定目标场景如何恢复。希望保留关卡状态时，不要误用 Recreate。离开设置场景会取消面板与窗口关联并隐藏停用 UI；缓存实例可再次使用。游戏自己的输入绑定与重新进入逻辑仍由游戏负责。

## 参考

- [Payload](../../../../engine/builtin/scenes/settings_scene_payload.h)、[可见项与草稿](../../../../engine/ui/presets/settings_panel.h)
- [运行时配置](../content/configuration.md)、[本地化](../systems/localization.md)
- [返回使用指南](../README.md)

## 窗口操作错误契约

窗口全屏、尺寸和位置操作均返回 `expected<void, FailureDiagnostic>`，在失败点捕获 SDL 错误和操作名。运行时先保存实际模式、窗口化尺寸和位置；应用失败后直接执行物理恢复，即使配置值没有变化也不会跳过。恢复中的各步骤继续尝试，原始失败和所有恢复失败同时保留并写入日志。失败操作不提交配置，也不返回 `Applied`。

启动和运行时复用相同的 SDL 操作适配与窗口快照恢复。读取实际窗口快照失败时，运行时修改在任何物理操作前被拒绝。启动全屏失败后仍尝试窗口模式、配置尺寸及居中位置；全部恢复成功才继续启动并记录全屏失败警告，任一步恢复失败则中止启动。

## 持久化与回滚诊断

`UserConfigFailure` 使用唯一的 `FailureDiagnostic`，不再提供重复的 `message`。`UserConfigCommitFailure::cause` 保留原始应用或写盘失败；其中可选的 `persistence` 描述磁盘恢复。`rollback_failure` 单独描述运行时设置回滚，继续尝试剩余设置并保留所有失败。磁盘恢复成功不代表运行时回滚成功，反之亦然。

配置写入显式检查临时文件写入、flush、close 和校验。主文件缺失时，已有有效 `.tmp` 先提升为主文件，再保存和轮换备份；无有效临时副本时保留已有备份。访问失败或未来版本临时副本禁止覆盖；访问失败不会触发损坏归档或默认值覆盖，归档失败则停止恢复。设置面板只显示本地化短提示“设置保存失败”；完整诊断写入日志，包含原始失败、磁盘恢复和运行时回滚结果及附加错误。失败后草稿仍从服务实际状态刷新。

保存前准备持久化快照，写盘成功后通过不分配的移动发布，失败不更新持久化基线。初始化准备完整状态后再发布；准备失败保留现有服务状态。非预期异常继续传播，渲染后端异常仍由应用故障退出边界接管。
