# 输入架构与生命周期

## 职责

| 模块 | 职责 |
| --- | --- |
| InputSystem | 按设备保存物理状态、帧初／帧末状态、有序事件、位移和设备变化；只输出快照 |
| GamepadDeviceManager | SDL 手柄设备打开、关闭与设备实例管理 |
| SceneInputRouter | 独立 UI 设备访问、原始操作消费、捕获、快捷键、玩家分流和释放／回中门控 |
| ControllerService | 游戏侧会话、创建、查询、配置、绑定和移除入口 |
| ControllerManager | 唯一持有控制器、命令缓存、句柄和固定步调度；Service 不维护副本 |
| SceneControlContext | 具体场景实例与代次，目标归属、活动性和模拟边界；不持有控制器 |
| Controller | 来源无关的意图生产契约 |
| LocalPlayerController | 本地玩家快照到游戏动作的转换 |
| ControlCommandReceiver | 角色解释动作并执行玩法 |

ControllerService 与 ControllerManager 复用引擎的 `elysia::tools::Singleton<T>`，使用继承的 `instance()`，禁止复制和移动；构造仍为私有，仅 Singleton 模板可创建实例。

普通 Scene 只有输入路由与 UI，不创建控制器上下文。GameplayScene 提供上下文和调度接入，也不自动创建任何控制器。游戏决定控制器数量、类型、映射和目标。

## 设备与玩家

物理源分为 `InputSourceId::keyboard()`、`mouse()` 和 `gamepad(instance)`，类别由 InputSourceKind 表达，不由编号范围推断。只有一个逻辑键盘和一个逻辑鼠标，不区分多个实体键鼠。各手柄转换器和扳机阈值独立；UI 自行记录活动设备用于提示；InputSystem 不提供全局 current_device 查询。

SceneManager 持有应用级 LocalPlayerRegistry。默认 P1 绑定完整键盘分区和鼠标。键盘分区是游戏配置，不是物理设备；每个玩家最多一个分区、一个鼠标源和一台手柄。分区之间不能占用同一个键，鼠标整体独占，玩家身份独立于设备和角色。

`create_partition`／`update_partition`／`remove_partition` 管理分区，`bind_keyboard` 绑定分区；`bind_source`／`unbind_source`／`transfer_source` 管理鼠标或手柄，`replace_gamepad` 替换玩家的手柄。上述设备变更统一返回 `expected<void, InputBindingError>`，有效但未分配的设备重复解绑成功，非法玩家或设备返回具体错误。`configuration` 提供只读配置，`apply_configuration` 原子提交批量变化，返回 InputBindingError；失败不改变绑定和版本。已绑定分区不能删除，活动控制器的键盘映射不能引用分区外按键。

每玩家 binding_version 在归属或分区内容变化时更新，输入阶段与实际 tick 前均检测。受影响玩家取消旧命令并要求持续控制释放／回中，其他玩家继续操作。鼠标转移丢弃待消费增量，不从当前位置合成位移。移除手柄只解绑该实例，重连不自动恢复。

同玩家按钮取并集、持续轴按映射叠加限幅；分区过滤帧初／帧末状态和有序事件。UI 消费始终按原始物理源和事件标识处理，再分流给玩家。

## 生命周期

SceneManager 在引擎侧初始化 Manager，关闭时结束会话并停用运行期；未初始化或关闭后 begin_session 返回 NotInitialized。

所有控制器属于显式游戏会话。`begin_session()` 不隐式覆盖已有会话；没有会话时创建失败。`end_session()` 取消并释放全部控制器，应用关闭也清理。游戏会话只是本地对象生命周期边界，与 ENet 或连接无关。

| 作用域 | 创建条件 | 离开场景／Reuse | Reset／Recreate／销毁 |
| --- | --- | --- | --- |
| Scene | 活动会话、有效所属上下文代次 | 取消、解绑，保留实例与配置 | 释放所属控制器 |
| Session | 活动会话 | 取消、解绑，保留实例与配置 | 保留实例，清除目标与旧上下文引用 |

场景 token 包含实例身份与代次，不能用 SceneKey 替代。Reset 递增代次，Recreate 产生新实例。新场景必须由游戏显式绑定，不自动寻找同名角色。

ControllerHandle 含运行期代次和实例号。移除、结束会话后旧句柄无效；新会话不会复用有效旧句柄。长期引用保存句柄，`get<T>()` 返回借用指针，不能跨调度修改边界保存。

一个控制器最多一个目标，一个目标最多一个控制器。同一活动场景、同一玩家最多一个已绑定本地控制器；允许未绑定和缓存场景实例共存。目标必须是当前活动上下文登记的存活对象。提交前校验失败保留原绑定；旧目标已收到取消后，若回调使新目标失效，则保持解绑并报告失败，不恢复旧命令。

回调内移除立即停止后续交付，实际析构延迟到安全边界；创建的新实例最早下一实际 tick 参与。回调内换绑、解绑和映射替换返回 Pending 操作结果，并在安全边界提交。入队与提交均校验，取消回调后再次校验；对象移除通知使待绑定请求立即失败。离开、换绑、重置清空状态、事件和增量，推进绑定代次。

## 一帧与一个 tick

1. Scene::on_input 委托 SceneInputRouter，维护物理源与设备变化。
2. UI 接受键鼠及指定 UI 手柄，与游戏绑定无关；逐操作前后重新检查捕获。
3. 未消费操作进入设备加入与场景快捷键；快捷操作用 set_shortcut_devices 声明设备类别，处理后 consume_input(event)。
4. 剩余快照进入对应 LocalPlayerController，动作映射结果进入 Manager 缓存。
5. PhysicsWorld 每次实际步前：自定义控制器产生意图、Manager 交付、游戏固定更新扩展、物理参与者更新及物理推进。

只使用已有 PhysicsWorld 累积器，无刚体也执行。零 tick 累计事件和增量，下一 tick 一次消费；补跑只保留持续值。丢弃的补步不消费输入。目标无效、inactive、上下文不活动时不积攒恢复后命令。

## UI 与取消

公共 UI 接受键盘、鼠标及最多一台独立指定的手柄。set_ui_gamepad 不修改游戏设备归属；纯菜单可以认领首个有效手柄按下作为 UI 手柄并消费该操作。玩法场景不自动认领。

普通 Scene 默认 Navigation；GameplayScene 默认 Pointer，HUD 的普通焦点不消费键盘／手柄导航。打开交互菜单使用 set_ui_interaction_mode(Navigation)，关闭恢复 Pointer。文本框获得焦点后即使处于 Pointer 模式也捕获整个物理键盘，影响所有键盘分区；鼠标捕获影响鼠标所属玩家，手柄捕获只影响指定 UI 手柄。一次原始操作产生的任何 UI 事件被消费，该操作及对应持续控制都不能穿透到玩法。

捕获默认只取消相关本地控制器。`set_all_gameplay_input_blocked` 屏蔽所有本地玩法输入，但不自动干预 AI／远端等自定义来源。世界 `pause()` 取消并停止所有控制器；UI 继续处理。

失焦、屏蔽、设备重绑、目标或映射切换使用取消通知，不能伪造普通释放事件。每个被屏蔽按键必须释放，摇杆／扳机须回到中立区（绝对值不超过 0.2）再生效。UI 操作权转移保留焦点，清理按住、拖拽、重复和合成滚动状态。

## 捕获与门控

通用 `InputCapture` 定义于 `engine/input/input_capture.h`，开发覆盖层和 UI 共用该类型。捕获期间匹配设备的持续值无条件清零，轴死区不能绕过捕获。解除捕获后，已有锁定仍需按键释放或每轴绝对值不超过 0.2 才能恢复。

`InputSuppression::observe` 仅按物理状态顺序更新门控，`filter` 是 const 查询；UI 帧预计算使用门控副本，不提前推进事件路由的真实门控。屏蔽本身不发送取消；路由按玩家合并取消，在映射前通知，原因优先级为 FocusLost、SourceChanged、Suppressed。暂停、解绑与不可用由控制器生命周期负责，持续不可用不重复通知。
