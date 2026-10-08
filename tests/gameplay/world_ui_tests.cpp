#include "tests/support/sdl_audio_fixture.h"
#define SDL_MAIN_HANDLED

#include "engine/gameplay/ui/world_bar.h"
#include "engine/gameplay/ui/speech_bubble.h"
#include "engine/core/render/render_command_projection.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"
#include "tests/support/test_assertions.h"
#include "game/showcase/gameplay/runtime/block_actor.h"

#include <SDL3_ttf/SDL_ttf.h>
#include <limits>
#include <type_traits>

namespace
{
using namespace elysia;
using namespace elysia::gameplay::ui;
using elysia::tests::require;

void test_bar()
{
    WorldBar bar;
    bar.set_range(10, 110);
    bar.set_value(35);
    require(bar.ratio() == 0.25f, "bar must normalize nonzero ranges");
    const core::Rect rect{20, 30, 100, 20};
    const WorldBarFillDirection directions[]{WorldBarFillDirection::LeftToRight,
        WorldBarFillDirection::RightToLeft, WorldBarFillDirection::TopToBottom,
        WorldBarFillDirection::BottomToTop};
    const core::Rect expected[]{{20,30,25,20}, {95,30,25,20}, {20,30,100,5}, {20,45,100,5}};
    for (std::size_t index = 0; index < 4; ++index)
    {
        std::vector<core::RenderCommand> commands;
        bar.set_fill_direction(directions[index]);
        bar.submit_render_commands(commands, rect);
        require(commands.size() == 2 && commands[1].command_rect == expected[index],
            "fill direction must preserve its fixed edge");
    }
    bar.set_ratio(2);
    require(bar.value() == 110, "ratio must clamp to full");
    bar.set_value(-10);
    require(bar.ratio() == 0, "values below range must produce empty fill");
    bar.set_value(std::numeric_limits<float>::quiet_NaN());
    bar.set_range(20, 0);
    require(bar.value() == 10, "invalid values and ranges must preserve valid state");
    bar.set_range(10, 10);
    require(bar.ratio() == 0, "zero span must remain empty and finite");
    bar.set_range(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    bar.set_ratio(0.5f);
    require(bar.ratio() == 0.5f, "wide finite ranges must not overflow");
    bar.set_style({{1,2,3,255},{4,5,6,255},{7,8,9,255},2});
    std::vector<core::RenderCommand> commands;
    bar.submit_render_commands(commands, rect);
    require(commands.size() == 3 && commands.back().type == core::RenderCommandType::DrawRect,
        "border must render after fill");
    camera::Camera camera({70,40}, {200,100}, 2);
    const auto projected = core::project_render_command_to_screen(commands.back(), camera);
    require(projected.screen_rect.width() == 200 && projected.stroke_width == 4,
        "bar geometry and border must scale with world camera");
    commands.clear();
    bar.set_visible(false);
    bar.submit_render_commands(commands, rect);
    require(commands.empty(), "hidden bars must not submit commands");
}

core::RenderCommand draw_text(const WorldText& text)
{
    std::vector<core::RenderCommand> commands;
    text.submit_render_commands(commands, {100,100}, {0.5f,1});
    require(commands.size() == 1, "visible text must submit one texture command");
    return commands.front();
}

void test_text(typography::FontResolver& resolver)
{
    static_assert(!std::is_copy_constructible_v<WorldText>);
    auto* manager = localization::LocalizationManager::instance();
    auto* service = ELYSIA_LOCALIZATION;
    WorldText first;
    WorldText second;
    first.set_text_key("gameplay_ui_demo.player");
    second.set_text_key("gameplay_ui_demo.player");
    first.set_color({210,50,20,128});
    second.set_color({20,220,40,255});
    const auto first_command = draw_text(first);
    const auto second_command = draw_text(second);
    require(first_command.texture == second_command.texture && first_command.alpha == 128
        && first_command.texture_color_modulation->r == 210,
        "tint mode must share white text across RGB and alpha variations");
    first.set_color_mode(TextColorMode::Baked);
    const auto baked = draw_text(first);
    require(baked.texture != second_command.texture && !baked.texture_color_modulation && baked.alpha == 128,
        "baked RGB must use a distinct texture with command-only opacity");
    first.set_color({210,50,20,64});
    require(draw_text(first).texture == baked.texture, "alpha changes must not allocate baked textures");
    manager->clear_texture_cache();
    require(draw_text(first).texture != nullptr, "key text must reacquire after shared cache invalidation");
    require(service->set_language("zh-Hans"), "language must switch");
    localization::LocalizedTextStyle style;
    style.color = {210,50,20,255};
    require(draw_text(first).texture == service->get_text_texture("gameplay_ui_demo.player", style),
        "key text must use the current localized texture");
    require(service->set_language("en"), "language must restore");

    WorldText raw;
    raw.set_raw_text("Original raw text");
    const auto original = draw_text(raw);
    require(draw_text(raw).texture == original.texture, "unchanged raw text must reuse owned texture");
    WorldText duplicate;
    duplicate.set_raw_text("Original raw text");
    require(draw_text(duplicate).texture != original.texture, "raw texture ownership must be per component");
    manager->clear_texture_cache();
    require(draw_text(raw).texture == original.texture, "shared cache clearing must not destroy raw textures");
    raw.set_color({50,180,240,100});
    require(draw_text(raw).texture == original.texture, "tint changes must reuse raw texture");
    raw.set_raw_text("Updated raw text");
    const auto updated = draw_text(raw);
    require(updated.texture != original.texture, "raw content changes must replace the owned texture");
    raw.set_typography_role(typography::UiTypographyRole::Title);
    const auto title = draw_text(raw);
    require(title.command_rect.height() > updated.command_rect.height(), "font role changes must rebuild raw text");
    require(service->set_language("zh-Hans"), "raw language must switch");
    require(draw_text(raw).texture != title.texture, "raw text must regenerate for active locale font");
    const auto full_size = raw.content_size();
    raw.set_world_units_per_pixel(0.5f);
    require(raw.content_size() == full_size * 0.5f, "world scale must affect both dimensions");
    raw.set_world_units_per_pixel(std::numeric_limits<float>::quiet_NaN());
    require(raw.content_size() == full_size * 0.5f, "invalid scales must not poison geometry");
    raw.set_visible(false);
    std::vector<core::RenderCommand> commands;
    raw.submit_render_commands(commands, {});
    require(commands.empty(), "hidden text must not draw");
    raw.set_visible(true);
    const auto generation = service->font_generation();
    resolver.shutdown();
    require(service->font_generation() != generation, "font shutdown must invalidate the font generation");
    raw.submit_render_commands(commands, {});
    require(commands.empty(), "raw text must not reuse an old generation when font resolution fails");
    const auto settings = typography::resolve_font_settings({});
    require(resolver.configure(*settings, *resources::ResourceService::instance(), service->supported_languages()),
        "fonts must restore");
    require(draw_text(raw).texture != nullptr, "failed texture resolution must remain retryable");
    raw.set_text_key("gameplay_ui_demo.enemy");
    require(draw_text(raw).texture != nullptr, "raw to key transition must resolve shared text");
    raw.set_raw_text("");
    commands.clear();
    raw.submit_render_commands(commands, {});
    require(commands.empty() && raw.content_size() == core::Vector2{}, "empty text must release content and draw nothing");
    require(service->set_language("en"), "restore locale for bubble tests");
}

void test_bubble(SDL_Renderer* renderer)
{
    SpeechBubble bubble;
    bubble.text().set_raw_text("A long line of dialogue that wraps above the character.");
    bubble.text().set_world_units_per_pixel(0.5f);
    bubble.text().set_max_width(80);
    const core::Vector2 tip{120,200};
    const auto wrapped_size = bubble.text().content_size();
    const auto body = bubble.body_rect(tip);
    require(body.center().x == tip.x && body.bottom() == tip.y - 6
        && body.width() == wrapped_size.x + 12 && body.height() == wrapped_size.y + 12,
        "bubble must derive body and tail geometry from text extent and padding");
    std::vector<core::RenderCommand> commands;
    bubble.submit_render_commands(commands, tip);
    require(commands.size() == 10 && commands[0].type == core::RenderCommandType::FillRect
        && commands[1].type == core::RenderCommandType::FillTriangle
        && commands.back().type == core::RenderCommandType::Texture,
        "bubble must draw background, tail, continuous outline, then text");
    require(commands[1].triangle_vertices[1] == tip
        && commands.back().command_rect.position() == body.position() + core::Vector2{6,6},
        "tail must reach owner anchor and text must respect padding");
    camera::Camera camera({120,120}, {320,240}, 1.5f);
    std::vector<core::ScreenRenderCommand> projected;
    core::project_render_commands_to_screen(commands, camera, projected);
    require(projected[1].triangle_vertices[1] == camera.world_to_screen(tip)
        && projected.back().screen_rect.size() == commands.back().command_rect.size() * 1.5f,
        "bubble and text must share one camera projection");
    require(SDL_SetRenderDrawColor(renderer, 0,0,0,255) && SDL_RenderClear(renderer), "clear render target");
    core::require_render_success(core::execute_render_commands(renderer, projected));
    require(SDL_RenderPresent(renderer), "world UI commands must execute on SDL renderer");
    bubble.text().set_max_width(0);
    require(bubble.text().content_size().y < wrapped_size.y, "removing wrapping must reduce multiline height");
    commands.clear();
    bubble.set_visible(false);
    bubble.submit_render_commands(commands, tip);
    require(commands.empty(), "hidden bubble must submit no primitives");
    bubble.set_visible(true);
    bubble.text().set_raw_text("");
    bubble.submit_render_commands(commands, tip);
    require(commands.empty(), "empty bubble must not leave a background behind");
}

void test_actor_integration()
{
    example::showcase::gameplay::BlockCombatActor minimum_health({
        .rect = {0,0,32,48}, .maximum_health = 0});
    std::vector<core::RenderCommand> minimum_commands;
    minimum_health.submit_render_commands(minimum_commands);
    require(minimum_commands.size() == 4
        && minimum_commands[1].command_rect == minimum_commands[2].command_rect,
        "bar initialization must use the health model's normalized maximum");
    example::showcase::gameplay::BlockCombatActor actor({
        .rect = {100,120,32,48},
        .team = gameplay::collision::teams::Player,
        .maximum_health = 100});
    std::vector<core::RenderCommand> commands;
    actor.submit_render_commands(commands);
    require(commands.size() == 4 && commands[1].command_rect == core::Rect{100,112,32,4}
        && commands.back().type == core::RenderCommandType::Texture,
        "actor must submit body, world bar, and localized nameplate");
    const auto name = commands.back().command_rect;
    const auto hit = actor.apply_damage({}, {.damage = 25});
    require(hit.remaining == 75, "demo damage must retain gameplay behavior");
    actor.set_position({140,150});
    commands.clear();
    actor.submit_render_commands(commands);
    require(commands.size() == 14 && commands[2].command_rect.width() == 24
        && commands[3].command_rect.position() == name.position() + core::Vector2{40,30}
        && commands[5].triangle_vertices[1] == core::Vector2{156,126},
        "damage must update fill and show a bubble following the owner's new position");
    actor.update(1.0);
    commands.clear();
    actor.submit_render_commands(commands);
    require(commands.size() == 14, "bubble must remain visible during its game-owned lifetime");
    actor.update(0.6);
    commands.clear();
    actor.submit_render_commands(commands);
    require(commands.size() == 4, "game update must hide expired speech without removing actor UI");
    actor.set_world_ui_visible(false);
    commands.clear();
    actor.submit_render_commands(commands);
    require(commands.size()==1,"showcase toggle hides every world UI primitive and retains the actor");
    actor.set_world_ui_visible(true);
    actor.show_speech();
    commands.clear();
    actor.submit_render_commands(commands);
    require(commands.size()==14,"showcase speech action reuses the actor bubble");
    (void)actor.apply_damage({}, {.damage=100});
    commands.clear();
    actor.submit_render_commands(commands);
    require(commands.size()==1,"dead targets stop submitting nameplate, health and bubble");
    minimum_health.destroy();
    minimum_commands.clear();
    minimum_health.submit_render_commands(minimum_commands);
    require(minimum_commands.size()==1,"retired targets stop submitting world UI before the safe point");
}
}

int main()
{
    test_bar();
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO), "initialize SDL");
    require(TTF_Init(), "initialize fonts");
    require(elysia::tests::open_test_mixer(), "initialize audio fixture");
    SDL_Surface* surface = SDL_CreateSurface(320,240,SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr, "create software surface");
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr, "create software renderer");
    auto* paths = elysia::io::PathManager::instance();
    require(paths->initialize(ELYSIA_SOURCE_DIR), "initialize source paths");
    const auto settings = elysia::typography::resolve_font_settings({});
    require(settings.has_value(), "resolve font settings");
    auto* builtin = elysia::builtin::BuiltinResources::instance();
    require(builtin->initialize(renderer, elysia::builtin::BuiltinAssetCatalog(*paths),
        settings->engine_point_sizes()), "load builtin fonts");
    elysia::typography::FontResolver resolver;
    auto* manager = elysia::localization::LocalizationManager::instance();
    require(manager->initialize(renderer, paths->configs() / "manifests/i18n_manifest.json", "en", &resolver),
        "initialize localization");
    require(resolver.configure(*settings, *elysia::resources::ResourceService::instance(),
        ELYSIA_LOCALIZATION->supported_languages()), "configure fonts");
    test_text(resolver);
    test_bubble(renderer);
    test_actor_integration();
    manager->shutdown();
    resolver.shutdown();
    elysia::resources::ResourceManager::instance()->clear();
    builtin->shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    elysia::tests::close_test_mixer();
    TTF_Quit();
    SDL_Quit();
}
