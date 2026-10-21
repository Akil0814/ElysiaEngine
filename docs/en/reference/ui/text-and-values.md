# Text, Input, and Value Display

This page separates content sources, typography/measurement, editing, and numeric display. Games supply data; existing services resolve fonts/translations.

See [UI guide](../../user-guide/ui/README.md#create-window) for integration and [index](README.md#browse-by-class) for classes.

<a id="text-content"></a>
## Content Sources and Measurement

`ui_text_key("path")` specifies localization; `ui_raw_text("...")` raw text. Both produce `UiTextContent`. `elysia::typography::UiTypographyRole` chooses fonts/sizes/default alignment; `TextHorizontalAlign`/`TextVerticalAlign` control position.

content_extent measures for parents. Wrapped height depends on current width and needs relayout after width changes; string length alone predicts neither font nor layout size.

<a id="ui-label"></a>
## UiLabel: Single-Line Text

`UiLabel` suits titles, button descriptions, short labels. Header: [ui_label.h](../../../../engine/ui/widgets/label/ui_label.h).

Provide `UiTextContent` at construction/`set_text_content`. Configure `set_horizontal_align`, `set_vertical_align`, `set_text_fit_mode`, `set_typography_role`, and `set_padding`.

`UiBlinkLabel`/`UiFadeLabel`/`UiPulseLabel` inherit these with opacity playback via `configure_playback`/`play`/`set_on_end`. See [text contracts](#text-content).

<a id="ui-text-block"></a>
## UiTextBlock: Multiline Text

`UiTextBlock` wraps to available width for body text/messages. Header: [ui_text_block.h](../../../../engine/ui/widgets/text/ui_text_block.h).

`set_text_content`/`clear_text` manage content; `set_typography_role`, `set_horizontal_align`, and `set_padding` configure measurement/drawing.

content_extent measures wrapped height at current width. Parent width changes require relayout; old heights may not fit. See [typography](#text-content).

<a id="ui-text-input"></a>
## UiTextInput: Single-Line Editing

`UiTextInput` handles UTF-8/IME editing. Header: [ui_text_input.h](../../../../engine/ui/widgets/ui_text_input.h).

`set_text`/text/`clear_text` manage contents; `set_placeholder_content`/`set_max_length` prompts/limits. Length counts UTF-8 code points; `nullopt` unlimited. Lowering limits truncates existing text. Cursor/composition are internal with no direct public setters.

`set_on_text_changed`/`set_on_submit` receive std::string_view valid during callbacks only; copy to string for retention. Programmatic `set_text`/`clear_text`/length changes also notify when final text changes; they are not silent. See [capture](input-and-focus.md#capture-and-cancel).

<a id="value-display"></a>
## Value Display: UiBar and UiNumber

`UiBar` expresses ratios; `UiNumber` lays out numeric text. Neither calculates health/cooldowns/business state.

Headers: [ui_bar.h](../../../../engine/ui/widgets/ui_bar.h), [ui_number.h](../../../../engine/ui/widgets/number/ui_number.h).

<a id="value-display-uibar"></a>
### UiBar

set_range(min,max)/`set_value` map values to fill; set_ratio directly sets [0,1]. `min_value`/`max_value`/`value`/`ratio` query results. `set_fill_direction` chooses four directions; `set_padding` reserves drawable insets.

<a id="value-display-uinumber"></a>
### UiNumber

<a id="value-display-数值精确整数与缺失值"></a>
#### Values, Exact Integers, and Missing Values

`set_value(double)` updates floating values; `set_integer_value(std::int64_t)` preserves every signed 64-bit digit. For scores/counts exceeding exact `double` range, use integers without converting first.

integer_value returns exact data only after integer assignment, even float `12.0` yields empty. `value` is a floating view and may lose large-integer precision; never round-trip exact integers through it.

clear_value means missing, displaying `--` without decimals/suffix. NaN/infinity also enter missing. has_value distinguishes missing from zero; missing `value` returns 0/integer_value empty. Finite/integer reassignment restores. Construction/reset use floating zero.

`set_decimal_places`, `set_trim_trailing_zeros`, `set_keep_decimal_point`, `set_suffix` format; formatted_text returns text. Integers obey formatting too, e.g. `42.00`. Percent appends % only; callers convert ratios. Always ASCII digits/decimal point independent of locale. Full `std::uint64_t` range is unsupported.

<a id="value-display-字形尺寸与区域适配"></a>
#### Glyph Size and Fitting

`set_digit_spacing`, `set_fixed_glyph_advance`, `set_target_height` control geometry; matching `clear_*` removes optional constraints. Fixed advance reserves per-glyph space plus spacing; advances smaller than width can overlap.

`set_shrink_to_fit(true)` proportionally shrinks all glyphs/spacing/advance into padded width/height. It never enlarges. Target height is pre-scale. Alignment uses scaled bounds without changing layout allocation. Large fixed advance retains trailing column space after the last glyph.

Shrinking defaults off and reset restores off; content may overflow. With fitting, more digits may shrink the string, so fixed advance cannot guarantee identical final screen columns across values.

Height/fitting affect geometry, not source/precision. See [presentation architecture](../../architecture/subsystems/ui/README.md#layout-presentation) for caches/commands.
