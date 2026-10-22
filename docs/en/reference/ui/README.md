# UI Interface Reference

This directory maintains public contracts by topic. Start with the [UI guide](../../user-guide/ui/README.md#create-window), then query by task or class. See [UI architecture](../../architecture/subsystems/ui/README.md#element-tree) for internal execution.

## Browse by Task

| Topic | Contents |
| --- | --- |
| [Elements and layout](elements-and-layout.md#ui-element) | Geometry, ownership, containers, fixed slots |
| [Input and focus](input-and-focus.md#navigation) | Navigation, capture, cancellation, extensions |
| [Buttons and selection](controls-and-selection.md#callbacks) | Buttons, HUD, groups, values, tabs |
| [Text and values](text-and-values.md#text-content) | Sources, editing, measurement, exact values |
| [Images and animation](media-and-animation.md#media-controls) | Textures, frame animation, translation, opacity |
| [Windows and popups](windows-and-popups.md#surface-contract) | Surfaces, dropdowns, dialogs, tooltips |
| [Styles and themes](style-and-theme.md#style-contract) | Overrides, registration, background roles |
| [Settings panel](settings-panel.md#settings-panel) | Visibility, drafts, options, saving |

## Browse by Class

Each topic presents common contracts before specific types. Links below locate each type directly. Enums, configuration, and callback results are documented with their owning type.

| Type | Location |
| --- | --- |
| `UiElement` | [View contract](elements-and-layout.md#ui-element) |
| `UiChildHost` | [View contract](elements-and-layout.md#ui-child-host) |
| `UiLayoutChildOptions`、`UiLayoutAnchor`、`UiLayoutPadding`、`UiLayoutMargin`、`UiLayoutAlign` | [View contract](elements-and-layout.md#fixed-layout) |
| `UiPanel` | [View contract](elements-and-layout.md#ui-panel) |
| `UiListContainer` | [View contract](elements-and-layout.md#ui-list-container) |
| `UiGridContainer` | [View contract](elements-and-layout.md#ui-grid-container) |
| `UiScrollContainer` | [View contract](elements-and-layout.md#ui-scroll-container) |
| `UiChromeContainer` | [View contract](elements-and-layout.md#ui-chrome-container) |
| `UiControl` | [View contract](input-and-focus.md#ui-control) |
| `UiFocusScope` | [View contract](input-and-focus.md#ui-focus-scope) |
| `UiControlFocusScopeHost` | [View contract](input-and-focus.md#ui-control-focus-scope-host) |
| `UiInputRouter`、`UiInputState`、`UiGamepadScrollSynthesizer`、`UiScrollState` | [View contract](input-and-focus.md#input-runtime) |
| `UiAction`、`UiInputEvent` | [View contract](input-and-focus.md#input-runtime) |
| `UiInteractionMode` | [View contract](input-and-focus.md#capture-and-cancel) |
| `UiButton` | [View contract](controls-and-selection.md#ui-button) |
| `UiActionButton` | [View contract](controls-and-selection.md#ui-action-button) |
| `UiCheckbox`、`UiRadioButton` | [View contract](controls-and-selection.md#selection-controls) |
| `UiButtonGroup`、`UiRadioGroup` | [View contract](controls-and-selection.md#selection-groups) |
| `UiDragHandle`、`UiSlider` | [View contract](controls-and-selection.md#drag-value-controls) |
| `UiLabeledCheckbox`、`UiLabeledRadioButton` | [View contract](controls-and-selection.md#labeled-controls) |
| `UiOptionSelector`、`UiOptionSelectorOrientation`、`UiOptionSelectorOption` | [View contract](controls-and-selection.md#ui-option-selector) |
| `UiTabBar`、`UiTabView`、`UiTabContainer` | [View contract](controls-and-selection.md#tabs) |
| `UiTextContent`、`UiTypographyRole` | [View contract](text-and-values.md#text-content) |
| `UiLabel` | [View contract](text-and-values.md#ui-label) |
| `UiTextBlock` | [View contract](text-and-values.md#ui-text-block) |
| `UiTextInput` | [View contract](text-and-values.md#ui-text-input) |
| `UiBar`、`UiNumber` | [View contract](text-and-values.md#value-display) |
| `UiImage`、`UiAnimation` | [View contract](media-and-animation.md#media-controls) |
| `UiFadeLabel`、`UiBlinkLabel`、`UiPulseLabel`、`UiFadeImage`、`UiBlinkImage`、`UiPulseImage` | [View contract](media-and-animation.md#opacity-variants) |
| `UiTranslationAnimationPlayer`、`UiOpacityFadeCore`、`UiOpacityBlinkCore`、`UiOpacityPulseCore` | [View contract](media-and-animation.md#animation-primitives) |
| `UiWindow` | [View contract](windows-and-popups.md#ui-window) |
| `UiOverlayOptions`、`UiOverlayPlacement`、`UiOverlayTransition` | [View contract](windows-and-popups.md#uioverlayoptions-placement-and-closing) |
| `UiTransientPopup` | [View contract](windows-and-popups.md#surface-contract) |
| `UiDialog`、`UiConfirmationDialog` | [View contract](windows-and-popups.md#dialogs) |
| `UiDropdown` | [View contract](windows-and-popups.md#ui-dropdown) |
| `UiTooltip` | [View contract](windows-and-popups.md#ui-tooltip) |
| `UiThemeManager`、`UiThemeRegistration`、`UiThemeDefaults` | [View contract](style-and-theme.md#style-contract) |
| `UiStyleState`、`UiThemeStyleResolver` | [View contract](style-and-theme.md#theme-runtime) |
| `SettingsPanel`、`SettingsPanelDraft`、`SettingsPanelOptions`、`SettingsPanelVisibility` | [View contract](settings-panel.md#settings-panel) |
