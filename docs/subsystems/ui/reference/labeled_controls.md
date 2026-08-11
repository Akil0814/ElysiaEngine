# 带标签选择控件

覆盖头文件：`ui_labeled_checkbox.h`、`ui_labeled_radio_button.h`。

两者把 indicator 与 Label 组合为单一可交互 control。构造时传对应 Config，或使用 `set_labeled_*_config` 更新文本、placement、spacing、base styles。共同布局 API：`set_label_placement`、`set_text_placement`、`set_label_spacing`、`set_typography_role`。

- LabeledCheckbox：`set_state`、`set_checked`、`toggle`、`set_on_toggled`；文本点击也会路由到 indicator。
- LabeledRadioButton：`set_selected`、`is_selected`、`set_on_selection_changed`；放入 `UiRadioGroup` 获得互斥。
