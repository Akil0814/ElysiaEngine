# UiActionButton

头文件：`engine/ui/widgets/ui_action_button.h`。用于 HUD 的通用动作槽位，可展示技能、物品格或暂停按钮。它继承 `UiElement`，接收鼠标事件，不参与焦点导航，不处理键盘／手柄 `Confirm`，也不绑定 gameplay action、玩家或 controller。

## 内容与状态

使用矩形、位置＋尺寸或 `from_center` 构造。`set_content` 接受空内容、`UiTextContent` 或 `UiActionButtonIconContent`；图标纹理由调用方持有，可指定 `source_rect` 显示图集区域。图标保持源区域宽高比并适配内部空间，文字使用现有本地化与字体服务。

- `set_key_hint` 设置左下角按键提示，`set_badge_text` 设置右上角角标；两者接受 raw text 或本地化 key。数量格式化与按键名称解析由 game 完成。
- `set_selected` 控制独立选中边框。
- `set_overlay_ratio` 控制从底部向上的矩形遮罩：0 无覆盖、1 完全覆盖；有限值限制到 `[0,1]`，非有限值按 0 处理。它不计时，也不影响是否可操作。
- `set_external_pressed` 只同步视觉状态，不执行回调。`is_pointer_pressed` 查询鼠标按住状态，`is_external_pressed` 查询外部状态，`is_pressed` 是两者的并集。普通鼠标释放不修改外部状态。
- `set_enabled(false)` 禁止新鼠标交互，并取消当前交互。系统或显式取消清除鼠标按住、hover 和外部按下表现；game 在下一次状态同步时可以重新提供外部状态。

绘制顺序为背景、内容、遮罩、普通边框、选中边框、提示、角标。遮罩与选择独立于启用状态。样式通过 `set_base_style`、`set_style_overrides` 和 `clear_style_overrides` 配置；主题管理器从按钮主题派生基础颜色，保留局部覆盖。`set_font_source_override` 可指定所有文字的字体来源，主文字使用 Button 排版角色，提示与角标使用 Caption。

控件采用布局坐标输出命令，由 Scene／父容器应用 presentation translation、透明度与裁剪；命中测试使用包含祖先位移的 presentation 坐标。

## 交互与生命周期

`set_on_interaction` 接收 `UiActionButtonInteraction`：

| 阶段 | 含义 |
| --- | --- |
| `Pressed` | 主鼠标键在可交互的控件内部按下；重复按下不重复通知。 |
| `Released` | 由本控件开始的鼠标交互结束；`clicked` 仅在释放时仍可交互且指针位于内部时为真。 |
| `Canceled` | 正在进行的鼠标交互因禁用、显式取消或系统输入取消而终止；不算成功点击。 |

移出控件不会结束按住，再移回后仍可成功点击；在外部释放也会通知 `Released`，但 `clicked=false`。鼠标移动和其他按键不被消费。控件不捕获整个设备类别，只消费自己开始的主鼠标按下／释放。

状态在回调前更新。回调可以移除当前控件、修改 UI 树或抛出异常；异常沿现有输入边界传播。批量系统取消使用稳定快照，继续清理其他仍存活的控件，再传播首次异常。取消回调中新增的控件不属于当前批次。

`reset()` 和析构不会执行回调。game 在隐藏、停用、移除 HUD 前，应显式调用 `cancel_input_interaction()`，并结束自身动作。game 的输入取消与场景生命周期也应清理游戏动作，不应依赖控件析构通知。控件允许暂停时接收 UI 输入；暂停期间的具体可操作条件由 game 通过启用状态与回调逻辑控制。

## game 层连接示例

可运行示例位于 UI 组件展示场景的「控件 / Controls」页，源码见 [ui_component_gallery_hud.cpp](../../../../../../game/showcase/ui/ui_component_gallery_hud.cpp)。E 或鼠标按下触发技能，3 秒后冷却恢复；1／2 或鼠标点击使用物品；P 或点击暂停按钮只暂停示例的冷却与物品操作。切换页签会清理交互状态，隐藏页不处理这些快捷键。

以下回调中的游戏方法由 game 自行实现。它们可以提交输入请求，也可以执行暂停等场景操作；控件自身不选择这些路径。

```cpp
using namespace elysia::ui;

// 按住型技能：点击 HUD 产生游戏输入请求，外部 gameplay 状态只更新视觉。
skill_button.set_key_hint(ui_raw_text("E"));
skill_button.set_on_interaction([this](const UiActionButtonInteraction& event) {
    if (event.phase == UiActionButtonInteractionPhase::Pressed)
        begin_skill_from_hud();
    else // Released 或 Canceled：均结束 HUD 产生的按住请求。
        end_skill_from_hud();
});
// game 的 HUD 更新：不要在这里重复执行技能。
skill_button.set_external_pressed(skill_action_is_held());
skill_button.set_overlay_ratio(skill_cooldown_remaining_fraction());
skill_button.set_enabled(can_start_skill());

// 物品格：一次成功点击改变游戏选中项；数量与选择由游戏状态反馈。
item_slot.set_on_interaction([this](const UiActionButtonInteraction& event) {
    if (event.phase == UiActionButtonInteractionPhase::Released && event.clicked)
        select_item_slot();
});
item_slot.set_badge_text(ui_raw_text(std::to_string(item_count())));
item_slot.set_selected(item_slot_is_selected());

// 暂停按钮：game 的键盘快捷键与 HUD 点击都调用同一个 game 方法。
pause_button.set_on_interaction([this](const UiActionButtonInteraction& event) {
    if (event.phase == UiActionButtonInteractionPhase::Released && event.clicked)
        toggle_pause();
});
```

按下就开始冷却的技能，应将“可开始新技能”与“当前按住是否继续”在 game 层区分：禁用控件会主动取消当前鼠标交互。多个输入来源共同控制按住动作时，game 应合并来源状态，避免一个来源释放终止另一个来源的按住。
