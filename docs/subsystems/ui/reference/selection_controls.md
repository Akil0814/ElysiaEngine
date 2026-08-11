# 基础选择控件：UiCheckbox 与 UiRadioButton

覆盖头文件：`widgets/ui_checkbox.h`、`widgets/ui_radio_button.h`。

## UiCheckbox

`UiCheckboxState` 有 `Unchecked`、`Checked`、`Indeterminate` 三态。`set_state` 直接写完整状态；`set_checked`、`is_checked`、`is_indeterminate` 是布尔快捷 API；`toggle` 推进状态。`set_on_toggled` 接收最终 `UiCheckboxState`，仅在用户交互引发变化时调用。

`set_mark_style` 选择 Checkmark、FilledBox 或 RadioDot；`set_state_textures`/`clear_state_textures` 管理可选纹理皮肤；`set_sounds` 管理交互声音。`UiCheckboxConfig` 可一次设置纹理、声音、style overrides 与 mark style。

## UiRadioButton

`set_selected`/`is_selected` 写入或读取选择状态；`select()` 请求选择并在状态变化时触发 `set_on_selection_changed`。它实现 `UiRadioItem`，但自身不清除同级 radio；需要互斥时放进 `UiRadioGroup`。

`UiRadioButtonConfig`、`set_radio_button_config`、sounds、padding、base style/overrides 的使用方式与 Checkbox 对应。Radio 的回调没有 index；由 Group 提供 index 级回调。
