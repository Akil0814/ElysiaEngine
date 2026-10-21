# UI 使用指南

本文从一个带确认弹窗的菜单说明布局、主题和焦点的组合方式。最小按钮见[UI 入门](README.md)，完整接口见[UI API 参考](../../reference/ui/README.md)。

## 最小窗口

下面是完整场景类型，可按[游戏初始化](../game-initialization.md)注册。窗口由场景持有，列表和弹窗由窗口持有，按钮由列表持有。示例不依赖仓库演示游戏的场景键或翻译资源。

```cpp
#include "engine/scene/scene.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/style/ui_theme_manager.h"
#include <stdexcept>

class GuideMenuScene final : public elysia::scene::Scene
{
protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        clear_menu(); // 本例选择每次进入重建菜单。
        elysia::ui::UiWindow* window = create_and_add_object<elysia::ui::UiWindow>(
            elysia::core::Rect{40, 40, 600, 360}, 100);
        window->set_padding({24.0f, 24.0f, 24.0f, 24.0f});
        elysia::ui::UiListContainer* actions =
            window->create_child<elysia::ui::UiListContainer>(
                elysia::ui::UiLayoutChildOptions{},
                elysia::core::Rect{0, 0, 220, 200});
        actions->set_direction(elysia::ui::UiListDirection::Vertical);
        actions->set_item_spacing(12.0f);

        elysia::ui::UiConfirmationDialog* dialog =
            window->create_child<elysia::ui::UiConfirmationDialog>(
                elysia::ui::UiLayoutChildOptions{});
        dialog->set_config({
            .title = elysia::ui::ui_raw_text("Exit game"),
            .message = elysia::ui::ui_raw_text("Return to desktop?"),
            .confirm = elysia::ui::ui_raw_text("Exit"),
            .cancel = elysia::ui::ui_raw_text("Cancel")});
        // 弹窗必须先成为窗口的直接子元素，再注册覆盖层。
        if (!dialog->register_with_window(*window))
        {
            throw std::runtime_error("Failed to register exit dialog.");
        }
        dialog->set_on_confirm([this]
        {
            request_quit();
        });

        elysia::ui::UiButton* exit_button = actions->create_child<elysia::ui::UiButton>(
            elysia::ui::UiLayoutChildOptions{},
            elysia::core::Rect{0, 0, 180, 44},
            elysia::ui::UiButtonConfig{.content = elysia::ui::ui_raw_text("Exit")});
        // 回调借用同一窗口拥有的弹窗；本例总是一起销毁整棵菜单树。
        exit_button->set_on_click([dialog]
        {
            dialog->open();
        });
        exit_button->set_visual_role(elysia::ui::UiButtonVisualRole::Primary);
        elysia::ui::UiButtonStyleOverrides overrides = exit_button->style_overrides();
        overrides.chrome.corner_radius = 8.0f;
        exit_button->set_style_overrides(overrides);

        window->register_focus_scope(*actions);
        window->focus_first_available_scope();
        _themes.set_theme(elysia::ui::UiBuiltinTheme::ElysiaDark);
        _theme_registration = _themes.register_root(*window);
    }

    void on_exit() override
    {
        clear_menu();
    }

    void on_reset() override
    {
        clear_menu();
    }

private:
    void clear_menu()
    {
        // 先解除主题关联，再清理窗口及其子树。
        _theme_registration.reset();
        clear_scene_objects();
    }

    elysia::ui::UiThemeManager _themes;
    elysia::ui::UiThemeRegistration _theme_registration;
};
```

## 场景、更新、输入与渲染

标准场景负责 UI 输入转换、更新和渲染命令执行；派生场景只组装控件和处理业务回调，不再次手动驱动 `UiInputRouter`。高 `order` 值绘制在上方，也优先接收输入。最终几何以 `screen_rect()` 为准，不在控件外维护另一套命中坐标。

`GameplayScene` 默认使用 Pointer 交互模式，菜单需要方向导航时显式切换到 Navigation。UI 手柄选择和玩法暂停见[GameplayScene 指南](../gameplay/scene.md#菜单与玩法输入)。内部路由过程见[UI 架构](../../architecture/subsystems/ui/architecture.md)。

## 布局和所有权

根窗口登记到场景，普通控件交给一个 `UiChildHost`。`add_child` 接收 `unique_ptr`，`create_child` 创建并交出所有权；返回裸指针只是借用。需要移动子项时，用 `extract_child` 取回所有权，再交给新容器。

上例使用竖向列表自动排列按钮。横向操作条可设置 Horizontal；规则网格使用 `UiGridContainer`。单项锚点、边距或尺寸覆盖由 `UiLayoutChildOptions` 配置，后续修改使用 `set_child_layout_options`。现有结构的公开字段实际名带下划线，例如 `._anchor`；不要为了调整示例命名而改写真实接口名。

容器会在更新或渲染需要时重建布局。滚动区域只持有一个内容节点，先把多项组织成列表或网格，再用 `UiScrollContainer::set_content` 交给滚动区域。

## 文本和本地化

上例用 `ui_raw_text` 显示固定文字，项目界面通常换成 `ui_text_key`，并在语言文件中提供对应键。用户输入使用原文来源，不能当成翻译键。按钮、标签、弹窗和下拉选项都通过 `UiTextContent` 表示来源，详见[本地化指南](../systems/localization.md)。

## 主题与局部样式

主题管理器向已注册根节点及后续加入的子树应用主题。上例把管理器和 `UiThemeRegistration` 都保存为场景成员；临时局部注册对象一离开作用域就会解绑。

视觉角色表达按钮用途，局部覆盖仅固定必要字段。`clear_style_overrides` 恢复完全接受主题；`set_base_style` 用于未受主题管理或需要明确基础样式的元素。复用场景时先解除旧注册，再注册新窗口。

## 焦点、键盘和手柄

列表、网格、面板等容器可作为焦点区域。先向窗口注册区域，再选中可用区域；区域内部管理按钮的方向邻居。多个区域使用 `register_focus_scope` 的 neighbors 参数配置左右或上下关系。

`Navigate*`、`Confirm` 和 `Cancel` 是设备无关 UI 动作。隐藏、禁用或销毁元素后，容器会同步可用焦点；不要长期保留旧 `focused_target()` 指针。窗口与区域的完整约定见[输入与焦点参考](../../reference/ui/concepts/input_focus_scroll.md)。

## 滚动、弹窗与提示

上例检查弹窗注册结果后才配置打开操作。确认弹窗关闭后再执行确认回调，模态层关闭时窗口尝试恢复先前焦点。普通 `UiDialog` 也遵循先归窗口所有、再注册、最后打开的顺序。

`UiDropdown` 的列表由控件自身持有，窗口仅协调弹出层；换窗口或销毁关联前解除注册。`UiTooltip` 是被动提示层，不接管玩法输入。滚动区域可通过 `scroll_by`、`scroll_to_*` 和 `ensure_visible` 控制显示位置，滚轮及手柄滚动由容器处理。

## 生命周期清单

- 子节点只有一个所有者；窗口销毁时一起释放其子树。
- 回调借用的对象必须活到回调解除或控件销毁；单独删除示例弹窗时，也要解除按钮中捕获它的回调。
- 保留主题注册对象；主题和窗口表面注册的详细解绑约定见[参考](../../reference/ui/concepts/window_surfaces.md)。
- `reset()` 会清理相应交互与子树状态，复用前按具体控件重新配置；不能假定旧回调或选择仍然存在。
