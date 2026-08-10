# UI Color Consolidation Audit

## Connected To `UiStyleDefaults`
- `UiLabel`
- `UiNumber`
- `UiBar`
- `UiPanel`
- `UiWindow`
- `UiButton`
- `UiCheckbox`
- `UiLabeledCheckbox` label sync
- `UiSlider`
- `UiTextInput`
- `UiDragHandle`
- `UiScrollContainer`
- `UiScrollBarStyle`

## Default Source Rules
- Compatibility defaults removed from interaction/style structs and config fallbacks.
- UiStyleDefaults is the only default visual source for color-bearing UI types.

## Audited As Structural / No Default Color Ownership
- `UiGridContainer`
- `UiListContainer`
- `UiImage`
- `UiBlinkImage`
- `UiFadeImage`
- `UiPulseImage`
