# GameplayScene 集成

普通菜单和加载场景继承 Scene，使用 on_shortcuts 和既有 UI 控件，不需要创建控制器。GameplayScene 为可控制世界提供 control_context() 和固定步调度接入，控制器数量完全由游戏决定。

## 显式创建与绑定

游戏先开始会话。在场景进入后创建或查询控制器，再绑定当前场景对象：

```cpp
using namespace elysia::gameplay;
auto* service = ControllerService::instance();
// 游戏进入游玩流程时执行一次，并检查错误：
auto session = service->begin_session();
// 场景已经活动，character 已登记；map 由游戏构造：
auto result = service->create<LocalPlayerController>(
    {ControllerScope::Scene, control_context().token()},
    elysia::input::PrimaryLocalPlayer, std::move(map));
if (result) {
    controller_handle = *result;
    auto binding = service->bind_target(controller_handle, control_context(), *character);
    // 游戏处理创建／绑定失败；不要丢弃错误而继续假定角色可控。
}
```

绑定拒绝错误场景、失效代次、销毁对象、非命令接收者和排他性冲突。跨场景控制器用 Session 作用域；新场景显式重新绑定。查询用句柄，不长期保存借用 Controller 指针。

## 引擎保证的接入

SceneManager 激活、退出、Reset 和 Recreate 时协调上下文。离开清理命令并解绑，Reuse 保留配置，Reset 释放 Scene 作用域；Session 作用域保留到 end_session。仅 reset_input_routing() 是 UI 路由重置，不能替代世界生命周期操作；游戏重启应请求 SceneReloadMode::Reset／Recreate。

对象释放前 Manager 解绑。GameplayScene 的输入、移除和固定步接入为 final，游戏无需也不能手动调用基类调度。游戏扩展使用 `on_game_fixed_update(tick, delta)`、`on_control_target_removing(object)`。每次实际固定步先交付命令，再执行游戏扩展，最后物理参与者和物理推进。

辅助工具通过游戏控制器显式处理；场景没有按玩家命令广播或转发钩子。多目标相机示例直接绑定实际可见 CameraActor，切换主目标显式换绑。

## UI 与暂停

Scene::on_input 仍是统一输入阶段，委托 SceneInputRouter。on_shortcuts 只获取 UI 所有者未消费的操作，处理后 consume_input(event)。on_unassigned_input 返回 true 消费加入操作。

set_ui_owner 转移公共 UI 操作权。set_all_gameplay_input_blocked 只屏蔽本地玩家玩法；pause 停止世界全部控制器并取消缓存。取消不是正常释放，恢复时按键和摇杆须满足释放／回中规则。

## 演示会话与双玩家

Demo Gallery 显式确保游戏会话存在；返回 Main Menu 结束会话；演示之间切换保留会话。直接启动示例的测试自行开始会话。单机完全不需要 ENet 会话。

Local multiplayer 示例使用游戏保存的 Session 作用域句柄。进入场景显式绑定蓝／红角色，返回演示选择页后实例仍存在但解绑，再进入时重新绑定；会话结束后旧句柄失效。

键鼠默认控制玩家 1，未绑定手柄 Start 加入玩家 2。公共菜单可更换手柄、转移 UI 所有者并测试文本捕获。示例只展示两个角色，底层注册表与控制器管理不限制两人。共享相机，不含分屏、联网或个人焦点树。
