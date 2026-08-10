# 页签组件：UiTabBar、UiTabView 与 UiTabContainer

覆盖头文件：`ui_tab_bar.h`、`ui_tab_view.h`、`ui_tab_container.h`。

- `UiTabBar` 管理 tab button：`add_tab`、`extract_tab`、`clear_tabs`、focused/selected index 与两个 callback。
- `UiTabView` 管理 page 所有权：`add_page`、`extract_page`、`clear_pages`、`set_selected_index`、`clear_selection`。
- `UiTabContainer` 是首选组合入口：`add_tab(label, page)` 返回 `UiTabAddResult`；失败时从 `rejected_page` 收回 page。`remove_tab` 返回已移除页面所有权。

TabBar 与 TabView 仅用于需要自定义组合的场景；大多数页面直接使用 TabContainer。
