#pragma once
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/label/ui_label.h"
#include <functional>

namespace example::showcase
{
// Scene owns the window and its children. This view stores only borrowed pointers.
class ShowcaseFrame final
{
public:
    void build(elysia::ui::UiWindow& window, const char* title, const char* description,
               std::function<void()> back, bool overlay = false);
    elysia::ui::UiButton* add_action(const char* key, std::function<void()> action);
    void set_status(elysia::ui::UiTextContent text);
    void clear() noexcept { _content = nullptr; _status = nullptr; }
    static void build_chrome(elysia::ui::UiWindow&,const char* title,const char* description,std::function<void()> back);
    elysia::ui::UiListContainer& content() const { return *_content; }
    static elysia::ui::UiLayoutChildOptions at(const elysia::core::Rect& rect);
private:
    elysia::ui::UiListContainer* _content = nullptr;
    elysia::ui::UiLabel* _status = nullptr;
};
}
