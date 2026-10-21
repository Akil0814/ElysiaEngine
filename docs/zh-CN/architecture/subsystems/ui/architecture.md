# 当前 UI 架构与实现思路

本文面向修改 UI 实现的开发者。它先解释元素、容器、输入和主题的分工，再按一帧流程说明协作关系；具体接口请使用组件参考页。

> 本文说明协作关系。逐类公开接口、调用顺序和所有权规则请从 [UI API 参考入口](../../../reference/ui/README.md) 进入对应组件页或专题页。

本文描述仓库当前实现，不是未来重构提案。

## 分层与依赖

```mermaid
flowchart LR
  Core[基础层： 元素、控件、所有权] --> Widgets[控件： 基础视觉与交互]
  Core --> Containers[容器： 布局、分组、滚动、页面]
  Layout[布局] --> Containers
  Input[输入] --> Focus[焦点]
  Focus --> Containers
  Style[样式与文字] --> Widgets
  Style --> Containers
  Widgets --> Composites[复合组件： 组合控件]
  Containers --> Composites
  Composites --> Window[窗口]
  Containers --> Window
  Window --> Scene[场景接入]
```

`UiElement` 提供矩形、绘制顺序、透明度和绘制指令提交；`UiControl` 在此基础上增加启用与焦点状态。`UiChildHost` 以 `unique_ptr` 拥有子树，并向子项传播更新、每帧输入、离散事件、渲染以及布局失效。容器不嵌入屏幕语义；复合组件通过拥有子控件和焦点委托组合功能，避免继承具体控件。

## 一帧的数据流

```mermaid
sequenceDiagram
  participant Raw as RawInput
  participant Router as UiInputRouter
  participant Window as UiWindow
  participant Overlay as 覆盖层与弹出层
  participant Scope as 焦点区域
  participant Leaf as 控件
  Raw->>Router: 帧状态与事件
  Router->>Window: UiInputFrame / UiInputEvent
  Window->>Overlay: 先路由打开的模态覆盖层
  Window->>Overlay: 再路由活动临时弹出层
  Window->>Scope: 当前焦点区域或指针命中的区域
  Scope->>Leaf: 导航、确认、指针、文本事件
  Leaf-->>Window: 是否已处理？
  Window->>Window: 更新/重建布局
  Window->>Leaf: 递归收集 UiRenderCommand
```

子节点按 UI 绘制顺序提交命令。`UiChildHost` 在需要时对一段子命令统一施加父级透明度与裁剪，所以内容矩形、裁剪与透明度可以组合而不要求每个子控件自行实现。布局以 `screen_rect`为输入/输出；子元素尺寸或内在尺寸变化会向父容器标脏，之后重建。

## 焦点与窗口层级

窗口保存注册的 `UiFocusScope` 及其四向邻居；焦点区域保存其控件与方向邻居。设备无关的`UiAction` 先在焦点区域内尝试导航，到边界后由窗口切换焦点区域。窗口还跟踪最近驱动焦点的设备，以区分指针与键盘/手柄策略。滚动容器作为嵌套焦点区域时会优先处理最深层可滚动内容，并在手柄滚动时暂时协调焦点显示。

覆盖层是窗口登记的子元素表面：模态层遮挡背景输入，关闭时可恢复原焦点区域；位置由`UiOverlayPlacement` 和窗口内容边界决定。临时弹出层不是子元素的所有权转移，而是控件向窗口提供一个渲染/输入协议；提示最后绘制且保持被动。

## 样式与主题

```mermaid
flowchart LR
  Theme[UiTheme / 内置主题] --> Resolver[UiThemeStyleResolver]
  Role[视觉角色] --> Resolver
  Resolver --> Base[组件 基础样式]
  Override[局部样式覆盖: 可选字段] --> State[UiStyleState]
  Base --> State
  State --> Render[控件渲染命令]
  Manager[UiThemeManager] --> Resolver
  Manager --> Tree[已注册 UiChildHost 子树]
```

主题由 `UiThemeManager` 应用于注册根及其后续子树。解析器使用元素类型与视觉角色选择主题样式；`UiStyleState` 将局部覆盖叠加到基础样式。复合组件内部子元素可标记为实现细节，以便继承复合组件的语义样式而不被当成独立主题根。该设计允许切换主题时保留调用方明确覆盖的字段。

## 关键约束

- `screen_rect` 是布局几何的依据；布局负责计算，输入和渲染消费结果。
- 公开返回的子元素指针不拥有对象；所有权始终在容器或复合组件。
- 注册关系是借用关系，必须随窗口/对象销毁解除；测试覆盖了覆盖层、弹出层、提示生命周期。
- UI 只生成 `UiRenderCommand`；实际 SDL 绘制位于 UI 模块之外的渲染基础设施。
