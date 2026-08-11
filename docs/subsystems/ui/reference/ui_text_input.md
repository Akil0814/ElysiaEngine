# UiTextInput

头文件：`engine/ui/widgets/ui_text_input.h`。单行 UTF-8/IME 输入控件。

调用：`set_text`、`text`、`clear_text` 管理 buffer；`set_placeholder_content`、`set_max_length` 管理约束；`set_on_text_changed` 与 `set_on_submit` 的 `string_view` 仅在回调期间有效。caret、选择与 IME composition 是内部编辑状态，不提供公开直接写入 API。
