#include "elysia_scene.h"

#include "../builtin/resources/builtin_asset_cache.h"
#include "../builtin/audio/builtin_audio_player.h"
#include "../builtin/resources/builtin_asset_keys.h"

#include "../input/raw_input_types.h"
#include "../scene/runtime/scene_runtime_context.h"

#include "../ui/widgets/image/ui_fade_image.h"
#include "../ui/widgets/label/ui_label.h"

#include "../ui/window/ui_window.h"
#include "../ui/containers/ui_list_container.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>


/*
「某一日，祂从天坠落。人们抬头仰望，于是看见了星空。」
「星月送来神的女儿，她愿成为人的伴侣。」
「长风化作她的轺车，四海落成她的园圃。鸟雀衔来善的种子，百花编织爱的颂歌。」
「她便是这样降生于世，行于大地，与人类一同长大，与世界一起发芽。」
「而今，终焉之时将至。」
「而今，归去之时已至。」
「就此告别吧，美丽的世界。」
「此后，将有群星闪耀，因为我如今来过。」
「此后，将有百花绽放，因为我从未离去。」
「请将我的箭、我的花、与我的爱，织成新生的种子，带向那枯萎的大地。」
「然后，便让它开出永恒而无瑕的…人性之华吧。」
「我名为爱莉希雅……」

「最初的律者，人之律者。」

我是律者，也是人类，但……更是一位如飞花般绚丽的少女呀♪


“ 不过，再厚的书，都会有翻到最后一页的时候;再难忘的故事，也都会有结束的时候呀。”
“ 一段故事结束，也是另一段故事的开始。只要那些名字不被遗忘，故事也会得到延续。”
“ 要微笑着看到最后一页哦。接下来的故事，就交给你来续写啦。”
*/
namespace elysia::realm
{
namespace
{
bool is_valid_return_route(const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}

constexpr std::array<std::string_view,9> kCodeLines = {
    "int main(int argc, char const *argv[])",
    "{",
    "    RestoreEgo();",
    "    RestorePurePinkHeart();",
    "    RestructureHerrscherOfHuman();",
    "    RestoreThirteenFlameChasers();",
    "    RebuildIncarnation();",
    "    return 0;",
    "}"
};

constexpr float kCodeListWidth = 280.0f;
constexpr float kCodeLineHeight = 20.0f;
constexpr float kCodeLineSpacing = 3.0f;
constexpr float kCodeListMargin = 16.0f;
constexpr double kCodeLineIntervalSeconds = 0.5;
}

void ElysiaScene::on_update(double delta)
{
    _code_timer.update(delta);
    elysia::scene::Scene::on_update(delta);
}

void ElysiaScene::on_input(const elysia::input::RawInputFrame& input,const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.control == elysia::input::RawInputControl::KeyEscape&& event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            return_to_caller();
            return;
        }
    }

    elysia::scene::Scene::on_input(input,events);
}

void ElysiaScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const ElysiaScenePayload* realm_payload =
        elysia::scene::try_scene_payload<ElysiaScenePayload>(payload);
    if (!realm_payload || !is_valid_return_route(realm_payload->return_route))
    {
        throw std::logic_error(
            "ElysiaScene requires ElysiaScenePayload with a valid return route.");
    }

    const auto* cache = runtime_context().builtin_asset_cache();
    if (!cache || !cache->initialized())
        throw std::logic_error("ElysiaScene requires an initialized BuiltinAssetCache.");

    const auto* audio_player = runtime_context().builtin_audio_player();
    if (!audio_player || !audio_player->bound())
        throw std::logic_error("ElysiaScene requires a bound BuiltinAudioPlayer.");

    _return_route = realm_payload->return_route;
    _paused = false;
    stop_playback();
    _playback_phase = PlaybackPhase::Loading;
    _current_line = 0;

    destroy_ui();
    build_ui();
    _root_window->set_visible(true);
    _root_window->set_active(true);

    if (!audio_player->play_music(elysia::builtin::asset_keys::ElysianRealm))
    {
        destroy_ui();
        throw std::logic_error("ElysiaScene Play Music Error.");
    }

    _code_timer.set_one_shot(false);
    _code_timer.set_wait_time(kCodeLineIntervalSeconds);
    _code_timer.set_on_timeout([this]() { reveal_next_code_line(); });
    _code_timer.restart();
}

void ElysiaScene::on_exit()
{
    stop_playback();
    const auto* audio_player = runtime_context().builtin_audio_player();
    if (audio_player && audio_player->bound())
        audio_player->stop_music();
    _paused = false;
    if (_root_window && !_root_window->is_destroyed())
    {
        _root_window->set_active(false);
        _root_window->set_visible(false);
    }
}

void ElysiaScene::reset()
{
    stop_playback();
    _playback_phase = PlaybackPhase::Loading;
    _current_line = 0;
    _paused = false;
    _return_route = {};
    destroy_ui();
}

void ElysiaScene::build_ui()
{
    const auto* cache = runtime_context().builtin_asset_cache();
    if (!cache)
        throw std::logic_error("ElysiaScene requires BuiltinAssetCache while building UI.");

    SDL_Texture* texture = cache->find_texture(
        elysia::builtin::asset_keys::ElysiaDefaultTexture);
    if (!texture)
        throw std::logic_error("ElysiaScene requires engine.brand.elysia.default.");

    _root_window = create_and_add_object<elysia::ui::UiWindow>(elysia::core::Rect{ 0,0,1280,720 },100);
    if (!_root_window)
        throw std::runtime_error("ElysiaScene could not create its UiWindow.");

    _root_window->set_on_cancel([this]() { return_to_caller(); });

    auto logo = std::make_unique<elysia::ui::UiFadeImage>(
        texture,
        elysia::core::Rect{ 0,0,320,320 });
    logo->configure_playback(
        elysia::ui::effects::UiOpacityFadeMode::FadeInOut,
        1.5,
        1.5,
        1.5);
    logo->set_on_end([this]() { ; });
    logo->play();
    _root_window->add_child(std::move(logo),
        elysia::ui::UiLayoutChildOptions{
            ._anchor = elysia::ui::UiLayoutAnchor::Center
        });

    _code_list = _root_window->create_child<elysia::ui::UiListContainer>(
        elysia::ui::UiLayoutChildOptions{
            ._anchor = elysia::ui::UiLayoutAnchor::BottomRight,
            ._margin = elysia::ui::UiLayoutMargin
            {0.0f,0.0f,kCodeListMargin,kCodeListMargin}
        },
        elysia::core::Rect{ 0,0,kCodeListWidth,0 });
    _code_list->set_direction(elysia::ui::UiListDirection::Vertical);
    _code_list->set_cross_align(elysia::ui::UiLayoutAlign::Start);
    _code_list->set_item_spacing(kCodeLineSpacing);

    _elysia_theme.set_theme(elysia::ui::UiBuiltinTheme::ElysiaLight);
    _theme_registration = _elysia_theme.register_root(*_root_window);
}

void ElysiaScene::destroy_ui() noexcept
{
    _theme_registration.reset();
    _code_list = nullptr;
    if (_root_window)
        _root_window->destroy();
    _root_window = nullptr;
}

void ElysiaScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}


void ElysiaScene::reveal_next_code_line()
{
    if (_current_line >= kCodeLines.size())
    {
        _playback_phase = PlaybackPhase::Complete;
        _code_timer.pause();
        return;
    }

    add_label(kCodeLines[_current_line]);
    ++_current_line;

    if (_current_line >= kCodeLines.size())
    {
        _playback_phase = PlaybackPhase::Complete;
        _code_timer.pause();
    }
}

void ElysiaScene::add_label(std::string_view code_line)
{
    if (!_code_list)
        throw std::runtime_error("ElysiaScene code list is null.");

    auto label = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0,0,kCodeListWidth,kCodeLineHeight },0,
        elysia::ui::ui_raw_text(std::string(code_line)));
    label->set_font_source_override(elysia::typography::FontSource::EngineBuiltIn);
    _code_list->add_back(std::move(label));
    _code_list->set_size(_code_list->content_extent());
}

void ElysiaScene::stop_playback() noexcept
{
    _code_timer.pause();
    _code_timer.set_on_timeout({});
}

}
