# 选择组：UiButtonGroup 与 UiRadioGroup

选择组将一组按钮或单选项组织为至多一个选中项，适合菜单和互斥设置。组负责协调选择，调用方读取选择结果。

相关头文件：`ui_button_group.h`、`ui_radio_group.h`。

两者都继承 `UiListContainer`，并维护至多一个选择项：

- `selected_index()` 返回 `std::optional<std::size_t>`；
- `set_selected_index(index)` 成功时返回 `true`；无效索引返回 `false`；
- `set_on_selection_changed` 接收新的可选索引。

ButtonGroup 使用 `add_button(std::unique_ptr<UiButton>)`，并有 `set_auto_select_first`；RadioGroup 接受作为子元素的 `UiRadioItem`（`UiRadioButton` 或 `UiLabeledRadioButton`）。不要同时手动修改子项选择和组选择来维持互斥。
