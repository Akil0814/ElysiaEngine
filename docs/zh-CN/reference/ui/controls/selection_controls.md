# 基础选择控件：UiCheckbox 与 UiRadioButton

复选框维护独立的三态值，单选按钮表达一项是否被选中。多个单选项之间的互斥由选择组维护，不由单个按钮自动完成。

相关头文件：`widgets/ui_checkbox.h`、`widgets/ui_radio_button.h`。

## UiCheckbox

`UiCheckboxState` 有 `Unchecked`、`Checked`、`Indeterminate` 三态。`set_state` 直接写完整状态；`set_checked`、`is_checked`、`is_indeterminate` 是布尔快捷 API；`toggle` 推进状态。`set_on_toggled` 接收最终 `UiCheckboxState`。用户交互和显式 `toggle()` 都会在状态改变时触发回调；`set_state()` 与 `set_checked()` 只同步状态，不触发回调。`toggle()` 在 Checked 与 Unchecked 之间切换，Indeterminate 切换为 Checked，不是三态轮流推进。带标签复选框沿用同一规则。

`set_mark_style` 选择 Checkmark、FilledBox 或 RadioDot；`set_state_textures`/`clear_state_textures` 管理可选纹理皮肤；`set_sounds` 管理交互声音。`UiCheckboxConfig` 可一次设置纹理、声音、局部样式覆盖与 mark 样式。

## UiRadioButton

`set_selected`/`is_selected` 写入或读取选择状态；`select()` 请求选择并在状态变化时触发 `set_on_selection_changed`。它实现 `UiRadioItem`，但自身不清除同级单选按钮；需要互斥时放进 `UiRadioGroup`。

`UiRadioButtonConfig`、`set_radio_button_config`、sounds、内边距、base style/overrides 的使用方式与 Checkbox 对应。单选按钮的回调不提供索引；需要知道选中了哪一项时，使用选择组的索引回调。
