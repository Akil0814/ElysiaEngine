# 推迟重构

# `UiWindow` 小范围组合式拆分计划

## Summary

只重构 `UiWindow` 及其窗口级协作逻辑；保留现有容器、浅继承、布局系统和 widget/composite API。`UiWindow` 继续是对外门面，现有场景调用语义保持不变；内部改为由注册表、四层 layer、焦点、popup、tooltip 与 mutation queue 组合完成。

## Window internals

- 新增 `UiNodeHandle { slot, generation }`，仅用于跨帧和跨回调引用；局部同步访问仍可使用现有裸指针。
- 新增 `UiWindowNodeRegistry`：
  - 窗口子树加入时递归注册，移除/销毁时先失效 handle。
  - 通过 `resolve(handle)` 返回当前存活节点；generation 不匹配一律视为失效。
  - 为外部样式子树保留现有管理方式，不强制其进入节点注册表。
- 新增 `UiWindowLayers`，由窗口拥有四个逻辑层：
  - `content`：普通子树。
  - `overlay`：modal 与 non-modal surface。
  - `popup`：dropdown 等 transient popup。
  - `tooltip`：被动提示。
  - layer 保存 `UiNodeHandle` 与层级元数据，不接管节点所有权；节点仍由原有 `UiChildHost` 或 composite 持有。
  - 渲染和输入均按四层执行，overlay/popup/tooltip 不再依赖裸指针注册项。
- 将 `UiWindow` 的实现拆分为私有协作者：
  - `UiWindowFocusController`
  - `UiWindowOverlayController`
  - `UiWindowPopupController`
  - `UiWindowTooltipController`
  - `UiWindowMutationQueue`
  - `UiWindow` 只保留生命周期编排、公开门面方法和四层最终 render/input 顺序。

## Handle migration and mutation safety

- 窗口长期状态全部改为 `UiNodeHandle`：
  - 当前/上次焦点 scope、scope 邻居、overlay 及其恢复焦点。
  - popup owner 与当前 popup。
  - tooltip 本体与 trigger。
- 需要调用 `UiFocusScope`、`UiTransientPopup`、`UiTooltip` 时，先通过 registry resolve，再 `dynamic_cast`；失效项在当前维护阶段删除。
- `UiWindow` 在 frame/event 分发期间进入 dispatch guard。
- 新增 `UiMutationQueue`，在 guard 期间收集 `append`、`remove`、`extract`、`clear`、`move`、`replace-content` 等结构操作；当前事件批次结束后，按入队顺序提交并统一清理注册、焦点和 layer 项。
- 现有局部容器 API 在非 dispatch 状态下立即执行；在 dispatch 状态下禁止直接结构修改并断言，调用方改用 `window.mutations()` 的 handle API。
- `UiMutationQueue::append` 在入队时分配 pending handle，提交后绑定节点；回调中不得依赖未提交节点的裸指针。

## Public behavior and migration

- 保留 `UiWindow` 的对外门面接口与现有场景行为；其内部改为委托给 controllers。
- `register_overlay`、popup 注册、tooltip 注册和 focus scope 注册在实现上立即转换为 handle-backed layer/controller 项。
- `UiTooltip` 的 trigger 改存 `UiNodeHandle`；`UiDropdown` 的窗口 popup 关联改存 owner handle；dialog/confirmation overlay 关联改存自身 handle。
- 更新 `UiChildHost` 的节点 attach/detach 钩子，使其能向最近的 `UiWindow` 注册表报告子树变化；不改变容器的继承结构。
- 删除 `UiWindow` 中现有的 raw-pointer vectors、raw focused-scope 指针、popup/tooltip/overlay 生命周期清理分支，并将相应逻辑移动到新协作者。
- 保留当前内容树渲染；layer controller 在最终 render pass 中负责排除/补绘已注册 surface，确保 overlay/popup/tooltip 不会重复绘制且位于 content 之上。

## Tests and acceptance

- 新增生命周期测试：
  - 回调中清空/替换包含 dropdown、dialog、tooltip 的子树，当前事件完成后无 UAF、无重复回调、无悬空 layer 项。
  - 节点销毁后旧 handle 无法 resolve，generation 复用不会误指向新节点。
  - overlay、popup、tooltip 的固定绘制顺序和输入优先级。
  - modal 关闭后按 handle 恢复焦点；目标已销毁时选择下一个可用 scope。
  - tooltip trigger 与 popup owner 移除后自动关闭并清理注册。
- 保留并更新现有 `ui_lifecycle_tests` 与 UI test scene，覆盖菜单、设置、角色选择的现有交互。
- 完成 CMake 重新生成、全量 Debug 构建、CTest 和 UI test scene 人工验证。
- 更新 `UI_REFACTOR_PLAN.md`，记录 handle 仅用于跨帧引用、回调结构修改必须走 mutation queue 的规则。

## Assumptions

- 不重构现有 container 继承、布局策略、style 系统或 widget API。
- overlay 也使用 handle，以统一四层生命周期安全。
- 现有用户可见功能和输入行为保持不变；变更仅限窗口内部实现与 callback 中的结构修改方式。
