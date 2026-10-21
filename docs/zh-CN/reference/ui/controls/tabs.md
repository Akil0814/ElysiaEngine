# 页签与页面：UiTabBar、UiTabView 和 UiTabContainer

页签界面需要同时管理选择按钮与页面内容。普通界面使用 `UiTabContainer` 即可；只有需要自定义组合时，才分别使用 `UiTabBar` 和 `UiTabView`。相关头文件为 `ui_tab_bar.h`、`ui_tab_view.h` 和 `ui_tab_container.h`。

## 组合入口

`UiTabContainer::add_tab(label, page)` 接收页面所有权并返回 `UiTabAddResult`。添加失败时，调用方从 `rejected_page` 收回页面；`remove_tab` 返回已移除页面的所有权。因此，失败或移除后仍需明确谁负责持有页面。

## 单独管理按钮和页面

`UiTabBar` 管理页签按钮，提供 `add_tab`、`extract_tab`、`clear_tabs`、焦点索引、选中索引及相应回调。`UiTabView` 持有页面，提供 `add_page`、`extract_page`、`clear_pages`、`set_selected_index` 和 `clear_selection`。

自定义组合时，需要自行协调按钮选择与页面选择，不能只切换其中一侧。
