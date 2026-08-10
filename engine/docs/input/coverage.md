# Input 公开头文件覆盖表

本表用于防止公开输入 API 新增后没有文档落点。实现细节 `.cpp` 不要求逐文件参考页，但文档中的数值和生命周期规则必须与实现一致。

| 公开头文件 | 主要类型/API | 文档位置 |
| --- | --- | --- |
| `engine/input/raw_input_types.h` | `RawInputControl`、`RawInputAxis`、`RawInputEvent` | [架构：输入设备与坐标约定](architecture.md#输入设备与坐标约定) |
| `engine/input/raw_input_state.h` | Control/Axis 当前状态与边沿查询 | [架构](architecture.md)、[测试与调试](testing-and-debugging.md) |
| `engine/input/raw_input_frame.h` | `RawInputFrame` | [架构：Frame 与 Event](architecture.md#frame-与-event-是两种读取方式) |
| `engine/input/action/input_action_types.h` | ID、descriptor、value、四种 binding、event phase | [Action Mapping](action-mapping.md) |
| `engine/input/action/action_input_frame.h` | Action frame 查询 | [Action Mapping：Frame 查询](action-mapping.md#frame-查询语义) |
| `engine/input/action/input_action_map.h` | 注册、解析、运行时改绑 | [Action Mapping：解析](action-mapping.md#解析与数值规则)、[运行时修改](action-mapping.md#运行时修改-binding) |
| `engine/gameplay/input/gameplay_actions.h` | 标准 Action IDs | [Engine Gameplay：标准 Actions](engine-gameplay.md#标准-actions) |
| `engine/gameplay/input/gameplay_input_map.h` | 默认 map 工厂 | [Engine Gameplay：默认 Bindings](engine-gameplay.md#默认-bindings) |
| `engine/gameplay/input/gameplay_input_frame.h` | `GameplayInputFrame` | [Engine Gameplay：GameplayInputFrame](engine-gameplay.md#gameplayinputframe) |
| `engine/gameplay/input/contracts/gameplay_input_frame_receiver.h` | Frame receiver | [Engine Gameplay：Receiver Contracts](engine-gameplay.md#receiver-contracts) |
| `engine/gameplay/input/contracts/gameplay_input_event_receiver.h` | Event receiver | [Engine Gameplay：Receiver Contracts](engine-gameplay.md#receiver-contracts) |
| `engine/gameplay/scene/gameplay_scene.h` | `GameplayScene` | [GameplayScene 集成指南](gameplay-scene.md) |

## 维护规则

- 新增公开 Action 值类型、binding 或标准 gameplay action 时，同步更新 Action Mapping 或 Engine Gameplay 表格。
- 修改 dead zone、threshold、epsilon、聚合或事件阶段时，同步更新规则说明与 input tests。
- 修改 GameplayScene 分发顺序、暂停或 enabled 行为时，同步更新场景指南和场景测试。
- 若加入持久化或 Context，应新增独立文档，不在当前页面提前描述未实现 schema。
