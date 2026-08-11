# UiLabel

头文件：`engine/ui/widgets/label/ui_label.h`。单行文本元素。

调用：构造或 `set_text_content` 提供 `UiTextContent`；`set_horizontal_align`、`set_vertical_align`、`set_text_fit_mode`、`set_typography_role`、`set_padding` 控制排版。`UiBlinkLabel`、`UiFadeLabel`、`UiPulseLabel` 继承此 API，并额外提供 `configure_playback`、`play`、`set_on_end`。
