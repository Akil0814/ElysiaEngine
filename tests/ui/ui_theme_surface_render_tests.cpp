#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/core/render/colors.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"
#include "engine/ui/composites/ui_dialog.h"
#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/presets/settings_panel.h"
#include "engine/ui/style/ui_theme_manager.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"

#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace elysia;
using tests::require;

struct ThemeCase
{
    ui::UiBuiltinTheme theme;
    const char* name;
    core::Color page;
    core::Color section;
    core::Color dialog;
};

constexpr std::array cases{
    ThemeCase{ui::UiBuiltinTheme::BlueGlassMoon,"blue-glass-moon",core::colors::abyss_blue,
        core::colors::midnight_blue,core::colors::midnight_blue},
    ThemeCase{ui::UiBuiltinTheme::ElysiaLight,"elysia-light",core::colors::elysia_chiffon_pink,
        core::colors::elysia_pearl_white,core::colors::elysia_silk_white},
    ThemeCase{ui::UiBuiltinTheme::ElysiaDark,"elysia-dark",core::colors::elysia_starsea_navy,
        core::colors::elysia_twilight_mist,core::colors::elysia_twilight_mist},
    ThemeCase{ui::UiBuiltinTheme::EvangelionUnit00,"evangelion-unit00",core::colors::eva_unit00_frost_blue,
        core::colors::eva_unit00_rei_white,core::colors::eva_unit00_rei_white},
    ThemeCase{ui::UiBuiltinTheme::EvangelionUnit01,"evangelion-unit01",core::colors::eva_unit01_void_purple,
        core::colors::eva_unit01_deep_purple,core::colors::eva_unit01_deep_purple},
    ThemeCase{ui::UiBuiltinTheme::EvangelionUnit02,"evangelion-unit02",core::colors::eva_unit02_pressed_maroon,
        core::colors::eva_unit02_deep_maroon,core::colors::eva_unit02_crimson},
    ThemeCase{ui::UiBuiltinTheme::QuietSlate,"quiet-slate",core::colors::quiet_slate_ink,
        core::colors::quiet_slate_charcoal,core::colors::quiet_slate_charcoal}
};

template<class T>
T* find_control(ui::UiChildHost& host)
{
    for (std::size_t i = 0; i < host.child_count(); ++i)
    {
        auto* child = host.child_at(i);
        if (auto* result = dynamic_cast<T*>(child))
            return result;
        if (auto* nested = dynamic_cast<ui::UiChildHost*>(child))
            if (auto* result = find_control<T>(*nested))
                return result;
    }
    return nullptr;
}

class RenderFixture
{
public:
    RenderFixture()
    {
        SDL_setenv_unsafe("SDL_VIDEO_DRIVER","dummy",1);
        SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),"SDL initializes");
        require(TTF_Init(),"SDL_ttf initializes");
        require(tests::open_test_mixer(),"test mixer opens");
        _surface = SDL_CreateSurface(1280,720,SDL_PIXELFORMAT_RGBA32);
        require(_surface,"software surface creates");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer,"software renderer creates");
        auto* paths = io::PathManager::instance();
        require(paths->initialize(ELYSIA_SOURCE_DIR),"project paths initialize");
        auto fonts = typography::resolve_font_settings({});
        require(fonts.has_value(),"font settings resolve");
        require(builtin::BuiltinResources::instance()->initialize(_renderer,
            builtin::BuiltinAssetCatalog(*paths),fonts->engine_point_sizes()).has_value(),
            "built-in fonts initialize");
        require(localization::LocalizationManager::instance()->initialize(_renderer,
            paths->configs()/"manifests"/"i18n_manifest.json","zh-Hans",&_fonts).has_value(),
            "localization initializes");
        require(_fonts.configure(*fonts,*resources::ResourceService::instance(),
            localization::LocalizationService::instance()->supported_languages()).has_value(),
            "font resolver configures");
    }

    ~RenderFixture()
    {
        localization::LocalizationManager::instance()->shutdown();
        _fonts.shutdown();
        builtin::BuiltinResources::instance()->shutdown();
        SDL_DestroyRenderer(_renderer);
        SDL_DestroySurface(_surface);
        tests::close_test_mixer();
        TTF_Quit();
        SDL_Quit();
    }

    void render(ui::UiWindow& window,const std::string& name)
    {
        window.update(0);
        std::vector<core::UiRenderCommand> commands;
        window.submit_ui_render_commands(commands);
        SDL_SetRenderDrawColor(_renderer,0,0,0,255);
        SDL_RenderClear(_renderer);
        require(core::execute_render_commands(_renderer,commands).has_value(),"UI commands render");
        require(SDL_RenderPresent(_renderer),"software renderer presents");
        if (const auto* directory = SDL_getenv("ELYSIA_THEME_QA_DIR"))
        {
            std::filesystem::create_directories(directory);
            require(IMG_SavePNG(_surface,(std::filesystem::path(directory)/(name+".png")).string().c_str()),
                "theme screenshot saves");
        }
    }

    core::Color pixel(int x,int y) const
    {
        Uint8 r{},g{},b{},a{};
        require(SDL_ReadSurfacePixel(_surface,x,y,&r,&g,&b,&a),"surface pixel reads");
        return {r,g,b,a};
    }

private:
    SDL_Surface* _surface = nullptr;
    SDL_Renderer* _renderer = nullptr;
    typography::FontResolver _fonts;
};

void test_settings_and_popups(RenderFixture& renderer)
{
    ui::UiWindow window(core::Rect{0,0,1280,720});
    auto owned = std::make_unique<ui::SettingsPanel>(core::Rect{0,0,732,680});
    auto* settings = owned.get();
    settings->set_options({.window_sizes={{1280,720},{1600,900},{1920,1080}},
        .target_fps_values={30,60,120,240},.languages={"zh-Hans","en","ja","ko","zh-Hant"}});
    settings->set_draft({.window_size={1280,720},.language="zh-Hans"});
    window.add_child(std::move(owned),{ui::UiLayoutAnchor::Center});
    settings->register_with_window(window);
    ui::UiThemeManager themes;
    auto registration = themes.register_root(window);
    auto* scroll = find_control<ui::UiScrollContainer>(*settings);
    auto* section = find_control<ui::UiPanel>(*settings);
    auto* dropdown = find_control<ui::UiDropdown>(*settings);
    require(scroll && section && dropdown,"settings has real scroll, section and dropdown controls");

    for (const auto& entry : cases)
    {
        themes.set_theme(entry.theme);
        const auto& theme = themes.current_theme();
        require(theme.window_style.background == entry.page
                && theme.panel(ui::UiPanelVisualRole::Screen).background == entry.page
                && theme.panel().background == entry.section
                && theme.panel(ui::UiPanelVisualRole::List).background == entry.section
                && theme.chrome_container_style.background == entry.section
                && theme.chrome_container_style.header_background == entry.page,
            "page and section backgrounds share the defined semantic mapping");
        require(theme.panel(ui::UiPanelVisualRole::Dialog).background == entry.dialog,
            "dialog-role backgrounds retain their original colors");
        require(scroll->style().background_color == core::colors::transparent,
            "runtime theme changes keep scroll backgrounds transparent");
        scroll->scroll_to_top();
        const std::string name = entry.name;
        renderer.render(window,name+"-settings-top");
        require(renderer.pixel(10,10) == entry.page,"actual page pixels use the theme page background");
        const auto rect = section->screen_rect();
        require(renderer.pixel(static_cast<int>(rect.x()+4),static_cast<int>(rect.center().y)) == entry.section,
            "actual section pixels remain distinct from the scroll host");
        const auto viewport = scroll->screen_rect();
        require(renderer.pixel(static_cast<int>(viewport.right()-10),static_cast<int>(viewport.center().y)) == entry.page,
            "scrollbar outer margin exposes the page rather than a filled scroll rectangle");
        scroll->scroll_to_bottom();
        renderer.render(window,name+"-settings-bottom");
        scroll->scroll_to_top();
        window.update(0);
        dropdown->open();
        require(dropdown->is_open(),"settings dropdown opens");
        std::vector<core::UiRenderCommand> popup;
        dropdown->submit_popup_render_commands(popup);
        require(!popup.empty() && popup.front().color == entry.section
                && popup.front().color.a == 255,
            "dropdown popup retains an opaque themed surface over transparent scrolling");
        renderer.render(window,name+"-settings-dropdown");
        dropdown->close();
    }
    settings->unregister_from_window();
}

void test_nested_scroll_dialog_and_overrides(RenderFixture& renderer)
{
    ui::UiWindow window(core::Rect{0,0,1280,720});
    auto host = std::make_unique<ui::UiPanel>(core::Rect{100,100,1080,520});
    auto* host_ptr = host.get();
    auto scroll = std::make_unique<ui::UiScrollContainer>(core::Rect{30,60,1020,400});
    scroll->set_scroll_axis(ui::UiScrollAxis::Vertical);
    auto rows = std::make_unique<ui::UiListContainer>(core::Rect{0,0,950,0});
    for (int i=0;i<16;++i)
        rows->add_back(std::make_unique<ui::UiLabel>(core::Rect{0,0,950,44},0,
            ui::ui_raw_text("滚动内容 · "+std::to_string(i+1))));
    scroll->set_content(std::move(rows));
    host->add_child(std::move(scroll));
    window.add_child(std::move(host));
    auto dialog = std::make_unique<ui::UiDialog>(core::Rect{0,0,660,460},10);
    auto* dialog_ptr = dialog.get();
    dialog_ptr->set_title_content(ui::ui_raw_text("阅读与背景分层"));
    std::string body;
    for (int i=0;i<10;++i)
        body += "滚动容器不再覆盖宿主底色，文字和按钮继续使用当前主题。\n";
    dialog_ptr->set_body_content(ui::ui_raw_text(body));
    dialog_ptr->set_action_content(ui::ui_raw_text("关闭"));
    window.add_child(std::move(dialog),{ui::UiLayoutAnchor::Center});
    require(dialog_ptr->register_with_window(window),"reading dialog registers");
    dialog_ptr->close();
    ui::UiThemeManager themes;
    auto registration = themes.register_root(window);
    for (const auto& entry : cases)
    {
        themes.set_theme(entry.theme);
        renderer.render(window,std::string(entry.name)+"-scroll");
        require(renderer.pixel(136,300) == entry.section,
            "nested scroll pixels expose the owning panel, not the page background");
        dialog_ptr->open();
        renderer.render(window,std::string(entry.name)+"-dialog");
        dialog_ptr->close();
    }
    const auto custom = core::colors::powder_blue;
    host_ptr->set_style_overrides(ui::UiPanelStyleOverrides{.background=custom});
    auto* nested = find_control<ui::UiScrollContainer>(*host_ptr);
    nested->set_style_overrides(ui::UiScrollContainerStyleOverrides{.background_color=custom});
    for (const auto& entry : cases)
    {
        themes.set_theme(entry.theme);
        require(host_ptr->style().background == custom && nested->style().background_color == custom,
            "explicit panel and scroll color overrides survive every theme change");
    }
    dialog_ptr->unregister_from_window();
}

void test_button_states(RenderFixture& renderer,const ThemeCase& entry)
{
    ui::UiWindow window(core::Rect{0,0,1280,720});
    ui::UiThemeManager themes;
    auto registration = themes.register_root(window);
    themes.set_theme(entry.theme);
    const auto add = [&](std::unique_ptr<ui::UiElement> child,const core::Rect& rect)
    {
        window.add_child(std::move(child),ui::UiLayoutChildOptions{
            ._margin = {.left=rect.x(),.top=rect.y()},
            ._size_override = rect.size(),
            ._use_size_override = true
        });
    };
    const auto label = [&](const char* text,const core::Rect& rect)
    {
        add(std::make_unique<ui::UiLabel>(rect,0,ui::ui_raw_text(text)),rect);
    };
    label(entry.name,{60,20,1120,50});
    constexpr std::array roles{ui::UiButtonVisualRole::Default,
        ui::UiButtonVisualRole::Primary,ui::UiButtonVisualRole::Danger};
    constexpr std::array names{"普通操作","主要操作","危险操作"};
    constexpr std::array states{"常态","悬停 / 聚焦","按下","禁用"};
    std::array<std::array<ui::UiButton*,4>,3> buttons{};
    for (std::size_t column=0;column<roles.size();++column)
    {
        const auto x = 260.0f+300.0f*static_cast<float>(column);
        label(names[column],{x,90,270,40});
        for (std::size_t row=0;row<states.size();++row)
        {
            const auto y = 160.0f+120.0f*static_cast<float>(row);
            if (column==0)
                label(states[row],{60,y,190,64});
            auto button = std::make_unique<ui::UiButton>(core::Rect{0,0,270,64});
            button->set_visual_role(roles[column]);
            button->set_text_content(ui::ui_raw_text(names[column]));
            auto* ptr = button.get();
            add(std::move(button),{x,y,270,64});
            if (row==1 || row==2)
                ptr->set_focused(true);
            if (row==2)
                (void)ptr->on_ui_input_event({.action=ui::UiAction::Confirm,
                    .type=ui::UiInputEventType::ActionPressed});
            if (row==3)
                ptr->set_enabled(false);
            buttons[column][row] = ptr;
        }
    }
    renderer.render(window,std::string(entry.name)+"-button-states");
    for (std::size_t column=0;column<roles.size();++column)
    {
        const auto& background = themes.current_theme().button(roles[column]).chrome.background;
        const std::array expected{background.idle,background.focused,background.active,background.disabled};
        for (std::size_t i=0;i<buttons[column].size();++i)
        {
            const auto rect = buttons[column][i]->screen_rect();
            require(renderer.pixel(static_cast<int>(rect.x()+8),static_cast<int>(rect.y()+8)) == expected[i],
                "real buttons must render the expected idle, focused, pressed and disabled colors");
        }
    }
}

void capture_idle_button_audit(RenderFixture& renderer)
{
    const auto hex = [](core::Color color)
    {
        std::ostringstream out;
        out << '#' << std::uppercase << std::hex << std::setfill('0')
            << std::setw(2) << static_cast<int>(color.r)
            << std::setw(2) << static_cast<int>(color.g)
            << std::setw(2) << static_cast<int>(color.b);
        return out.str();
    };
    std::ofstream report;
    if (const auto* directory = SDL_getenv("ELYSIA_THEME_QA_DIR"))
    {
        report.open(std::filesystem::path(directory)/"button-idle-audit.csv");
        require(report.is_open(),"button audit report opens");
        report << "theme,role,page,section,fill,border,text\n";
    }
    constexpr std::array roles{ui::UiButtonVisualRole::Default,
        ui::UiButtonVisualRole::Primary,ui::UiButtonVisualRole::Danger};
    constexpr std::array names{"普通操作","主要操作","危险操作"};
    constexpr std::array role_names{"Default","Primary","Danger"};
    for (const auto& entry : cases)
    {
        ui::UiWindow window(core::Rect{0,0,1280,720});
        ui::UiThemeManager themes;
        auto registration = themes.register_root(window);
        themes.set_theme(entry.theme);
        const auto add = [&](std::unique_ptr<ui::UiElement> child,const core::Rect& rect)
        {
            window.add_child(std::move(child),ui::UiLayoutChildOptions{
                ._margin={.left=rect.x(),.top=rect.y()},
                ._size_override=rect.size(),._use_size_override=true});
        };
        const auto label = [&](const std::string& text,const core::Rect& rect)
        {
            add(std::make_unique<ui::UiLabel>(rect,0,ui::ui_raw_text(text)),rect);
        };
        label(std::string(entry.name)+" / idle",{24,8,1232,40});
        label("页面背景 "+hex(entry.page),{24,58,600,32});
        label("分区背景 "+hex(entry.section),{660,58,596,32});
        auto section = std::make_unique<ui::UiPanel>(core::Rect{0,0,608,116});
        section->set_style_overrides(ui::UiPanelStyleOverrides{.corner_radius=10.0f,.draw_border=false});
        add(std::move(section),{648,102,608,116});
        for (std::size_t column=0;column<roles.size();++column)
        {
            const auto& style = themes.current_theme().button(roles[column]);
            require(style.chrome.background.idle != entry.page
                    && style.chrome.background.idle != entry.section,
                "every idle button role must have a fill distinct from page and section backgrounds");
            if (report.is_open())
                report << entry.name << ',' << role_names[column] << ',' << hex(entry.page)
                    << ',' << hex(entry.section) << ',' << hex(style.chrome.background.idle)
                    << ',' << hex(style.chrome.border.idle) << ',' << hex(style.text.enabled) << '\n';
            for (const auto origin : {24.0f,660.0f})
            {
                const auto x = origin+196.0f*static_cast<float>(column);
                auto button = std::make_unique<ui::UiButton>(core::Rect{0,0,184,56});
                button->set_visual_role(roles[column]);
                button->set_text_content(ui::ui_raw_text(names[column]));
                add(std::move(button),{x,118,184,56});
                label(hex(style.chrome.background.idle),{x,179,184,30});
            }
        }
        renderer.render(window,std::string(entry.name)+"-buttons-idle");
    }
}
}

int main()
{
    RenderFixture renderer;
    test_settings_and_popups(renderer);
    test_nested_scroll_dialog_and_overrides(renderer);
    for (const auto& entry : cases)
        test_button_states(renderer,entry);
    capture_idle_button_audit(renderer);
    return EXIT_SUCCESS;
}
