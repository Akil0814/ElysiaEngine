# 选择组：UiButtonGroup 与 UiRadioGroup

覆盖头文件：`ui_button_group.h`、`ui_radio_group.h`。

两者都继承 `UiListContainer`，并维护至多一个选择项：

- `selected_index()` 返回 `std::optional<std::size_t>`；
- `set_selected_index(index)` 成功时返回 `true`；无效 index 返回 `false`；
- `set_on_selection_changed` 接收新的 optional index。

ButtonGroup 使用 `add_button(std::unique_ptr<UiButton>)`，并有 `set_auto_select_first`；RadioGroup 接受作为 child 的 `UiRadioItem`（`UiRadioButton` 或 `UiLabeledRadioButton`）。不要同时手动修改 item 选择和 group 选择来维持互斥。
