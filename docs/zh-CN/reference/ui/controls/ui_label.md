# UiLabel：单行文本

`UiLabel` 显示单行文字，适合标题、按钮说明和短标签。头文件为 `engine/ui/widgets/label/ui_label.h`。

构造时或通过 `set_text_content` 提供 `UiTextContent`。`set_horizontal_align`、`set_vertical_align`、`set_text_fit_mode`、`set_typography_role` 和 `set_padding` 配置排版。

需要透明度动画时，可使用继承这些接口的 `UiBlinkLabel`、`UiFadeLabel` 或 `UiPulseLabel`，再通过 `configure_playback`、`play` 和 `set_on_end` 配置播放。文本与字体的共同规则见[文本专题](../concepts/text_number.md)。
