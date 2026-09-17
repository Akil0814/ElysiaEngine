# 输入测试与调试

## 自动化验证

- `controller_runtime_tests`：显式会话、作用域、上下文代次、回调内修改、自定义来源、映射替换及鼠标增量。
- `input_system_controller_lifecycle_tests`：多个手柄按钮、摇杆、扳机独立；键鼠并行；移除与失焦通知。
- `gameplay_scene_input_tests`：玩家绑定、目标排他性、快速点按、零／多 tick、屏蔽恢复、重绑、销毁、队列溢出与物理步前回调。
- `development_input_capture_tests`：开发面板捕获、文本框捕获传播、逐玩家隔离、UI 操作权切换、按原始操作消费及失焦恢复。
- `input_action_mapping_tests` 与 `engine_gameplay_tests`：动作注册、换绑、轴值和游戏层命令视图；默认玩法动作不属于引擎通用协议。
- 原有指针坐标、UI、场景、物理及角色测试继续覆盖迁移后的调用关系。

```powershell
ctest --test-dir out/build/physics-scenarios-msvc -C Debug -L input --output-on-failure
```

自动测试通过可控 SDL 输入事件复现多设备，不需要真实手柄。真实双手柄验收需要另行在硬件上确认，不能用合成输入测试替代实机结论。

## 排查顺序

1. 检查快照中源 ID 与物理状态，确认不同手柄没有混在一起。
2. 检查 `LocalPlayerRegistry::owner(source)` 和角色绑定，未绑定输入不会控制角色。
3. 检查 UI 所有者、捕获类别和回中门控，观察输入是否被 UI 消费。
4. 检查 ControllerHandle、绑定代次、本地动作映射及命令 events／deltas，区分持续值与一次性动作。
5. 检查实际执行 tick 和取消原因，不使用渲染帧数推断执行次数。

命令事件每 tick 消费一次；同帧多次点击会保留多条事件。菜单关闭后仍按住的键不会立即恢复，需要先释放；这属于明确行为。若队列溢出，先检查模拟是否长期停步以及输入是否错误地重复注入。

## 双玩家示例与硬件验收

从 Demo Gallery 进入 **Local multiplayer / input routing**。默认键鼠属于玩家 1；未绑定手柄按 Start 加入玩家 2。加入按键会被消费，不会同时打开菜单。方向键／左摇杆控制对应角色，状态栏显示玩家、设备和 UI 所有者；该示例共享一台相机。

要测试双手柄，打开 **Players / menu**，选择 **Bind next Start controller to player 1**，再在另一只未绑定手柄按 Start。玩家 1 可以同时保留键鼠和手柄。需要交换已绑定手柄时，先选择 **Release controllers for reassignment**，再分别使用玩家 1 分配和玩家 2 重绑操作。拔掉手柄只解除该设备绑定，重新连接不会按设备名称恢复归属。

公共菜单可显式切换 UI 所有者。菜单打开时所有玩家的玩法输入被屏蔽，文本框用于检查输入不会穿透。关闭菜单后，持续按键必须释放，摇杆／扳机必须回中，才能恢复。切换 UI 所有者保留焦点，但旧所有者正在按住的确认操作不能在新所有者下完成。

实机验收应覆盖键鼠＋手柄、双手柄同时移动、拔插、菜单确认／文本输入、所有者切换及恢复中立状态。当前自动化使用可控 SDL 事件；真实双手柄交互尚未执行。

## 上一轮基线记录（不能作为本轮通过结论）

2026-09-16，Windows / MSVC Debug，构建目录 `out/build/physics-scenarios-msvc`：

- 完整构建成功。
- 完整 CTest：120 / 120 通过，包含 Input、UI、Scene、Physics、Builtin、示例和物理压力测试。
- 双玩家示例经过可控 SDL 输入验证和软件渲染截图检查：角色可见、加入不触发菜单、两名玩家独立移动、模态菜单屏蔽玩法。
- 修改文档的相对链接检查和 `git diff --check` 通过。
- 未执行真实双手柄交互验收；没有实现或验证网络会话。

## 本轮控制器子系统重构验证记录

2026-09-17，Windows / MSVC Debug，构建目录 `out/build/physics-scenarios-msvc`：

- 完整构建成功；本轮新增 `controller_runtime_tests`。最终完整 CTest：121 / 121 通过，用时 112.62 秒，日志位于 `out/controller-full-tests-confirmed.log`。
- 前一次完整运行中 `scenario_stress_contacts_2` 超时，其余 120 项通过；该项随后单独复跑通过（53.66 秒），最终完整复跑也通过（该项 46.85 秒）。超时原因未确定，没有修改测试超时阈值或跳过压力测试。
- 自动化覆盖显式会话与句柄失效、场景／会话作用域、回调内创建／移除／换绑、自定义非设备控制器、映射替换、固定 tick 消费及鼠标增量。
- 补充验证普通 HUD 消费鼠标移动时只清除对应待消费增量，保留未消费的滚轮和按钮事件；命令数值与事件前值的非有限值均被拒绝。
- 查询工具通过同一套活动映射响应键盘与手柄，验证零 tick 等待、多 tick 不重复执行以及缓存场景重新进入后的绑定。
- 使用可控 SDL 输入重新运行 `demo_scene_tests` 并导出软件渲染截图至 `out/controller-routing-qa`。已检查双玩家运行画面、公共模态菜单及多目标相机画面，设备归属提示、角色和控件显示正常。
- 本轮涉及文档的 28 个本地链接有效；旧玩家命令、输入广播、场景控制目标接口和活动手柄排他字段扫描无残留；`git diff --check` 通过。
- 真实双手柄交互未执行。合成 SDL 输入与软件渲染检查不代表实机验收；本轮不包含 ENet 会话或网络复制实现。
