# UiTextInput：单行文字输入

`UiTextInput` 处理单行 UTF-8 文本与输入法编辑。头文件为 `engine/ui/widgets/ui_text_input.h`。

使用 `set_text`、`text` 和 `clear_text` 管理内容，使用 `set_placeholder_content` 和 `set_max_length` 设置提示与长度约束。光标、选区和输入法组合文字属于内部编辑状态，没有公开的直接写入接口。

`set_on_text_changed` 和 `set_on_submit` 的参数为 `std::string_view`，只在回调执行期间有效。若要在回调结束后保留文字，必须复制为 `std::string`。键盘捕获规则见[输入运行时](input_runtime.md)。
