# MoonLine Input 开发者文档

本目录描述当前已经实现的输入链，以 `engine/input` 与 `engine/gameplay` 的代码为事实来源。文档面向维护输入引擎、扩展标准 gameplay action，以及编写实际游玩场景的开发者。

完整数据流为：

```text
SDL event
  -> RawInputFrame / RawInputEvent
  -> InputActionMap
  -> ActionInputFrame / ActionInputEvent
  -> GameplayInputFrame
  -> GameplayScene receiver
```

UI 与 gameplay 并不共用同一种语义事件。基础 `Scene` 将 Raw Input 转换成 UI Input；只有 `GameplayScene` 会额外执行 Action Mapping 和 gameplay receiver 分发。

## 阅读入口

- [架构与一帧输入流](architecture.md)：分层职责、frame/event 双通道、UI 与 gameplay 分流。
- [Action Mapping 详解](action-mapping.md)：Action ID、值类型、binding、dead zone、事件阶段与运行时改绑。
- [Engine Gameplay 参考](engine-gameplay.md)：标准 actions、默认键位、语义访问器和项目扩展方式。
- [GameplayScene 集成指南](gameplay-scene.md)：场景继承、receiver、排序、暂停、禁用和事件消费。
- [测试与调试](testing-and-debugging.md)：现有测试覆盖和常见问题排查。
- [公开头文件覆盖表](coverage.md)：公开类型到文档章节的映射。

## 建议阅读顺序

第一次接触本系统时，依次阅读架构、Action Mapping 和 GameplayScene。只需要添加一个游戏动作时，可直接查阅 Engine Gameplay 的“扩展 Action”；排查摇杆或事件边沿问题时，使用测试与调试页。

## 当前边界

当前实现提供运行时注册、替换、清除和恢复默认 binding，但不负责磁盘持久化；没有 Input Context 栈；右摇杆没有标准 gameplay 语义；鼠标位置、滚轮和文本输入继续由 Raw/UI 输入链处理。
