# 带标签选择控件

这两种控件将选择标记与文字标签组合起来，适合设置项。标签属于同一交互控件，读者点击文字时也能触发对应选择。

相关头文件：`ui_labeled_checkbox.h`、`ui_labeled_radio_button.h`。

两者把选择标记与标签组合为单一交互控件。构造时传对应 Config，或使用 `set_labeled_*_config` 更新文本、位置关系、间距和基础样式。共同布局 API：`set_label_placement`、`set_text_placement`、`set_label_spacing`、`set_typography_role`。

- LabeledCheckbox：`set_state`、`set_checked`、`toggle`、`set_on_toggled`；文本点击也会路由到选择标记。
- LabeledRadioButton：`set_selected`、`is_selected`、`set_on_selection_changed`；放入 `UiRadioGroup` 获得互斥。
