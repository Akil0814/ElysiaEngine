#define SDL_MAIN_HANDLED

#include "engine/core/render/sdl_render_command_executor.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace
{
using elysia::tests::require;

class SdlStrokeFixture
{
public:
    SdlStrokeFixture(int width,int height)
        : _width(width),
          _height(height)
    {
        SDL_SetHint(SDL_HINT_RENDER_LOGICAL_SIZE_MODE,"letterbox");
        require(SDL_Init(SDL_INIT_VIDEO) == 0,
            "stroke rendering tests must initialize SDL video");
        _window = SDL_CreateWindow(
            "ui stroke rendering test",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            width,
            height,
            SDL_WINDOW_HIDDEN);
        require(_window != nullptr,
            "stroke rendering tests must create a hidden window");
        _renderer = SDL_CreateRenderer(
            _window,-1,SDL_RENDERER_SOFTWARE);
        require(_renderer != nullptr,
            "stroke rendering tests must create a software renderer");
        require(SDL_RenderSetLogicalSize(_renderer,1280,720) == 0,
            "stroke rendering tests must configure the logical canvas");
        _format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
        require(_format != nullptr,
            "stroke rendering tests must allocate a pixel format");
        clear();
    }

    ~SdlStrokeFixture()
    {
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
        SDL_FreeFormat(_format);
        SDL_Quit();
    }

    void clear()
    {
        SDL_SetRenderDrawColor(_renderer,0,0,0,0);
        SDL_RenderClear(_renderer);
    }

    void read_pixels()
    {
        _pixels.assign(
            static_cast<std::size_t>(_width * _height),
            std::uint32_t{});
        require(
            SDL_RenderReadPixels(
                _renderer,
                nullptr,
                SDL_PIXELFORMAT_RGBA32,
                _pixels.data(),
                _width * static_cast<int>(sizeof(std::uint32_t))) == 0,
            "stroke rendering tests must read renderer pixels");
    }

    [[nodiscard]] bool visible(int x,int y) const noexcept
    {
        if (x < 0 || y < 0 || x >= _width || y >= _height)
            return false;
        const std::uint32_t pixel = _pixels[
            static_cast<std::size_t>(y * _width + x)];
        Uint8 r = 0;
        Uint8 g = 0;
        Uint8 b = 0;
        Uint8 a = 0;
        SDL_GetRGBA(pixel,_format,&r,&g,&b,&a);
        return a != 0 && (r != 0 || g != 0 || b != 0);
    }

    [[nodiscard]] elysia::core::Color pixel_color(int x,int y) const noexcept
    {
        if (x < 0 || y < 0 || x >= _width || y >= _height)
            return {};
        const std::uint32_t pixel = _pixels[
            static_cast<std::size_t>(y * _width + x)];
        Uint8 r = 0;
        Uint8 g = 0;
        Uint8 b = 0;
        Uint8 a = 0;
        SDL_GetRGBA(pixel,_format,&r,&g,&b,&a);
        return {r, g, b, a};
    }

    [[nodiscard]] elysia::core::Vector2 window_point(
        const elysia::core::Vector2& logical) const noexcept
    {
        const elysia::core::Vector2 snapped =
            elysia::core::detail::snap_ui_point_to_output_pixel(
                _renderer,logical);
        float scale_x = 1.0f;
        float scale_y = 1.0f;
        SDL_RenderGetScale(_renderer,&scale_x,&scale_y);
        return {
            std::round(snapped.x * scale_x),
            std::round(snapped.y * scale_y)
        };
    }

    [[nodiscard]] int inward_run(int x,int y,int step_x,int step_y) const noexcept
    {
        int count = 0;
        while (visible(x,y))
        {
            ++count;
            x += step_x;
            y += step_y;
        }
        return count;
    }

    [[nodiscard]] SDL_Renderer* renderer() const noexcept
    {
        return _renderer;
    }

private:
    int _width = 0;
    int _height = 0;
    SDL_Window* _window = nullptr;
    SDL_Renderer* _renderer = nullptr;
    SDL_PixelFormat* _format = nullptr;
    std::vector<std::uint32_t> _pixels;
};

constexpr elysia::core::Color k_white{ 255,255,255,255 };

void require_hairline_rect(int output_width,int output_height)
{
    using namespace elysia::core;
    SdlStrokeFixture fixture(output_width,output_height);
    const Rect rect{ 100.4f,100.4f,200.2f,120.2f };
    execute_render_command(
        fixture.renderer(),
        make_ui_draw_rect_command(rect,k_white,rect,0.0f));
    fixture.read_pixels();

    const Vector2 top_left = fixture.window_point(rect.top_left());
    const Vector2 bottom_right = fixture.window_point(rect.bottom_right());
    const int left = static_cast<int>(top_left.x);
    const int top = static_cast<int>(top_left.y);
    const int right = static_cast<int>(bottom_right.x) - 1;
    const int bottom = static_cast<int>(bottom_right.y) - 1;
    const int center_x = (left + right) / 2;
    const int center_y = (top + bottom) / 2;

    const int left_run = fixture.inward_run(left,center_y,1,0);
    const int right_run = fixture.inward_run(right,center_y,-1,0);
    const int top_run = fixture.inward_run(center_x,top,0,1);
    const int bottom_run = fixture.inward_run(center_x,bottom,0,-1);
    require(left_run == 1,
        "hairline rect left side must be one output pixel");
    require(right_run == 1,
        "hairline rect right side must be one output pixel");
    require(top_run == 1,
        "hairline rect top side must be one output pixel");
    require(bottom_run == 1,
        "hairline rect bottom side must be one output pixel");

    for (int x = left; x <= right; ++x)
    {
        require(fixture.visible(x,top),
            "hairline rect top side must be continuous");
        require(fixture.visible(x,bottom),
            "hairline rect bottom side must be continuous");
    }
    for (int y = top; y <= bottom; ++y)
    {
        require(fixture.visible(left,y),
            "hairline rect left side must be continuous");
        require(fixture.visible(right,y),
            "hairline rect right side must be continuous");
    }
}

void require_curved_and_line_strokes(int output_width,int output_height)
{
    using namespace elysia::core;
    SdlStrokeFixture fixture(output_width,output_height);

    const Rect rounded{ 100,80,220,120 };
    execute_render_command(
        fixture.renderer(),
        make_ui_draw_rect_command(rounded,k_white,24.0f));

    const Vector2 circle_center{ 500,140 };
    constexpr float circle_radius = 54.0f;
    execute_render_command(
        fixture.renderer(),
        make_ui_draw_circle_command(circle_center,circle_radius,k_white));

    const Vector2 line_start{ 650,100 };
    const Vector2 line_end{ 900,180 };
    execute_render_command(
        fixture.renderer(),
        make_ui_draw_line_command(line_start,line_end,k_white));
    fixture.read_pixels();

    const Vector2 rounded_top = fixture.window_point(rounded.top_center());
    const Vector2 rounded_bottom = fixture.window_point(rounded.bottom_center());
    const Vector2 rounded_left = fixture.window_point(rounded.center_left());
    const Vector2 rounded_right = fixture.window_point(rounded.center_right());
    require(fixture.visible(
            static_cast<int>(rounded_top.x),static_cast<int>(rounded_top.y)),
        "rounded rect top side must be visible");
    require(fixture.visible(
            static_cast<int>(rounded_bottom.x),static_cast<int>(rounded_bottom.y) - 1),
        "rounded rect bottom side must be visible");
    require(fixture.visible(
            static_cast<int>(rounded_left.x),static_cast<int>(rounded_left.y)),
        "rounded rect left side must be visible");
    require(fixture.visible(
            static_cast<int>(rounded_right.x) - 1,static_cast<int>(rounded_right.y)),
        "rounded rect right side must be visible");

    const Vector2 circle_top = fixture.window_point(
        { circle_center.x,circle_center.y - circle_radius });
    const Vector2 circle_bottom = fixture.window_point(
        { circle_center.x,circle_center.y + circle_radius });
    const Vector2 circle_left = fixture.window_point(
        { circle_center.x - circle_radius,circle_center.y });
    const Vector2 circle_right = fixture.window_point(
        { circle_center.x + circle_radius,circle_center.y });
    require(fixture.visible(
            static_cast<int>(circle_top.x),static_cast<int>(circle_top.y)),
        "circle top side must be visible");
    require(fixture.visible(
            static_cast<int>(circle_bottom.x),static_cast<int>(circle_bottom.y) - 1),
        "circle bottom side must be visible");
    require(fixture.visible(
            static_cast<int>(circle_left.x),static_cast<int>(circle_left.y)),
        "circle left side must be visible");
    require(fixture.visible(
            static_cast<int>(circle_right.x) - 1,static_cast<int>(circle_right.y)),
        "circle right side must be visible");

    for (int i = 0; i <= 20; ++i)
    {
        const float t = static_cast<float>(i) / 20.0f;
        const Vector2 logical = line_start + (line_end - line_start) * t;
        const Vector2 device = fixture.window_point(logical);
        bool found = false;
        for (int y = static_cast<int>(device.y) - 1;
             y <= static_cast<int>(device.y) + 1;
             ++y)
        {
            for (int x = static_cast<int>(device.x) - 1;
                 x <= static_cast<int>(device.x) + 1;
                 ++x)
            {
                found = found || fixture.visible(x,y);
            }
        }
        require(found,"hairline line stroke must remain continuous");
    }
}

int logical_stroke_output_width(int output_width,int output_height)
{
    using namespace elysia::core;
    SdlStrokeFixture fixture(output_width,output_height);
    const Rect rect{ 100,100,200,120 };
    execute_render_command(
        fixture.renderer(),
        make_ui_draw_rect_command(
            rect,
            k_white,
            0.0f,
            UiStrokeWidth{ UiStrokeWidthMode::Logical,2.0f }));
    fixture.read_pixels();
    const Vector2 top_left = fixture.window_point(rect.top_left());
    const Vector2 center_left = fixture.window_point(rect.center_left());
    return fixture.inward_run(
        static_cast<int>(top_left.x),
        static_cast<int>(center_left.y),
        1,
        0);
}

void test_logical_stroke_scales_and_renderer_state_is_restored()
{
    require(logical_stroke_output_width(960,540)
            < logical_stroke_output_width(1600,900),
        "logical two-pixel strokes should visibly scale with the canvas");

    using namespace elysia::core;
    SdlStrokeFixture fixture(1280,720);
    SDL_SetRenderDrawColor(fixture.renderer(),11,22,33,44);
    SDL_SetRenderDrawBlendMode(fixture.renderer(),SDL_BLENDMODE_ADD);
    const SDL_Rect original_clip{ 5,6,700,500 };
    SDL_RenderSetClipRect(fixture.renderer(),&original_clip);

    execute_render_command(
        fixture.renderer(),
        make_ui_draw_circle_command(
            Vector2{ 200,200 },
            50.0f,
            k_white,
            Rect{ 100.2f,100.2f,200.2f,200.2f }));

    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    SDL_GetRenderDrawColor(fixture.renderer(),&r,&g,&b,&a);
    require(r == 11 && g == 22 && b == 33 && a == 44,
        "stroke rendering must preserve renderer draw color");
    SDL_BlendMode blend_mode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(fixture.renderer(),&blend_mode);
    require(blend_mode == SDL_BLENDMODE_ADD,
        "stroke rendering must preserve renderer blend mode");
    SDL_Rect restored_clip{};
    SDL_RenderGetClipRect(fixture.renderer(),&restored_clip);
    require(restored_clip.x == original_clip.x
            && restored_clip.y == original_clip.y
            && restored_clip.w == original_clip.w
            && restored_clip.h == original_clip.h,
        "stroke rendering must preserve renderer clip state");
}

void test_world_primitives_render_and_restore_state()
{
    using namespace elysia::core;
    SdlStrokeFixture fixture(1280,720);

    ScreenRenderCommand fill_rect;
    fill_rect.type = RenderCommandType::FillRect;
    fill_rect.screen_rect = {20, 20, 40, 30};
    fill_rect.color = {255, 0, 0, 128};
    execute_render_command(fixture.renderer(), fill_rect);

    ScreenRenderCommand draw_rect;
    draw_rect.type = RenderCommandType::DrawRect;
    draw_rect.screen_rect = {100, 20, 40, 30};
    draw_rect.color = k_white;
    draw_rect.stroke_width = 3.0f;
    execute_render_command(fixture.renderer(), draw_rect);

    ScreenRenderCommand fill_circle;
    fill_circle.type = RenderCommandType::FillCircle;
    fill_circle.circle_center = {200, 40};
    fill_circle.circle_radius = 15.0f;
    fill_circle.color = k_white;
    execute_render_command(fixture.renderer(), fill_circle);

    ScreenRenderCommand draw_circle;
    draw_circle.type = RenderCommandType::DrawCircle;
    draw_circle.circle_center = {260, 40};
    draw_circle.circle_radius = 15.0f;
    draw_circle.stroke_width = 2.0f;
    draw_circle.color = k_white;
    execute_render_command(fixture.renderer(), draw_circle);

    ScreenRenderCommand line;
    line.type = RenderCommandType::DrawLine;
    line.line_start = {320, 20};
    line.line_end = {350, 50};
    line.stroke_width = 2.0f;
    line.color = k_white;
    execute_render_command(fixture.renderer(), line);
    fixture.read_pixels();

    require(fixture.visible(40, 35),
        "World FillRect must rasterize with alpha blending enabled");
    require(fixture.visible(100, 35),
        "World DrawRect must rasterize its requested stroke");
    require(fixture.visible(200, 40),
        "World FillCircle must rasterize its center");
    require(fixture.visible(260, 25),
        "World DrawCircle must rasterize its circumference");
    require(fixture.visible(335, 35),
        "World DrawLine must rasterize between its endpoints");

    SDL_SetRenderDrawColor(fixture.renderer(),11,22,33,44);
    SDL_SetRenderDrawBlendMode(fixture.renderer(),SDL_BLENDMODE_ADD);
    const SDL_Rect original_clip{5, 6, 700, 500};
    SDL_RenderSetClipRect(fixture.renderer(),&original_clip);
    execute_render_command(fixture.renderer(), fill_circle);

    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    SDL_GetRenderDrawColor(fixture.renderer(),&r,&g,&b,&a);
    require(r == 11 && g == 22 && b == 33 && a == 44,
        "World primitive rendering must preserve renderer draw color");
    SDL_BlendMode blend_mode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(fixture.renderer(),&blend_mode);
    require(blend_mode == SDL_BLENDMODE_ADD,
        "World primitive rendering must preserve renderer blend mode");
    SDL_Rect restored_clip{};
    SDL_RenderGetClipRect(fixture.renderer(),&restored_clip);
    require(restored_clip.x == original_clip.x
            && restored_clip.y == original_clip.y
            && restored_clip.w == original_clip.w
            && restored_clip.h == original_clip.h,
        "World primitive rendering must preserve renderer clip state");
}

void test_world_triangles_render_and_restore_state()
{
    using namespace elysia::core;
    SdlStrokeFixture fixture(1280,720);

    ScreenRenderCommand background;
    background.type = RenderCommandType::FillRect;
    background.screen_rect = {10, 10, 180, 80};
    background.color = {0, 0, 255, 255};
    execute_render_command(fixture.renderer(),background);

    SDL_SetRenderDrawColor(fixture.renderer(),11,22,33,44);
    SDL_SetRenderDrawBlendMode(fixture.renderer(),SDL_BLENDMODE_ADD);
    const SDL_Rect original_clip{0, 0, 5, 5};
    SDL_RenderSetClipRect(fixture.renderer(),&original_clip);

    ScreenRenderCommand clockwise;
    clockwise.type = RenderCommandType::FillTriangle;
    clockwise.triangle_vertices = {
        Vector2{20, 20},Vector2{80, 20},Vector2{50, 80}
    };
    clockwise.color = {255, 0, 0, 128};
    execute_render_command(fixture.renderer(),clockwise);

    ScreenRenderCommand counter_clockwise;
    counter_clockwise.type = RenderCommandType::FillTriangle;
    counter_clockwise.triangle_vertices = {
        Vector2{120, 20},Vector2{150, 80},Vector2{180, 20}
    };
    counter_clockwise.color = {0, 255, 0, 255};
    execute_render_command(fixture.renderer(),counter_clockwise);
    fixture.read_pixels();

    const Color blended = fixture.pixel_color(50,40);
    require(blended.r > 0 && blended.b > 0,
        "FillTriangle must alpha-blend its uniform color over prior content");
    const Color outside = fixture.pixel_color(20,75);
    require(outside.r == 0 && outside.g == 0 && outside.b > 0,
        "FillTriangle must not color pixels outside its edges");
    const Color reverse_winding = fixture.pixel_color(150,40);
    require(reverse_winding.g > reverse_winding.r
            && reverse_winding.g > reverse_winding.b,
        "FillTriangle must render both clockwise and counter-clockwise vertices");

    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    SDL_GetRenderDrawColor(fixture.renderer(),&r,&g,&b,&a);
    require(r == 11 && g == 22 && b == 33 && a == 44,
        "FillTriangle must preserve renderer draw color");
    SDL_BlendMode blend_mode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(fixture.renderer(),&blend_mode);
    require(blend_mode == SDL_BLENDMODE_ADD,
        "FillTriangle must preserve renderer blend mode");
    SDL_Rect restored_clip{};
    SDL_RenderGetClipRect(fixture.renderer(),&restored_clip);
    require(restored_clip.x == original_clip.x
            && restored_clip.y == original_clip.y
            && restored_clip.w == original_clip.w
            && restored_clip.h == original_clip.h,
        "FillTriangle must restore renderer clip state after ignoring it");
}

void test_texture_and_world_primitive_submission_order()
{
    using namespace elysia::core;
    SdlStrokeFixture fixture(1280,720);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr,
        "Mixed world rendering test must create a source surface");
    SDL_FillRect(surface, nullptr,
        SDL_MapRGBA(surface->format, 0, 0, 255, 255));
    SDL_Texture* texture = SDL_CreateTextureFromSurface(
        fixture.renderer(), surface);
    SDL_FreeSurface(surface);
    require(texture != nullptr,
        "Mixed world rendering test must create a texture");

    ScreenRenderCommand primitive;
    primitive.type = RenderCommandType::FillTriangle;
    primitive.triangle_vertices = {
        Vector2{500, 100},Vector2{520, 100},Vector2{510, 120}
    };
    primitive.color = {255, 0, 0, 255};
    ScreenRenderCommand textured;
    textured.type = RenderCommandType::Texture;
    textured.texture = texture;
    textured.screen_rect = {500, 100, 20, 20};

    execute_render_command(fixture.renderer(), primitive);
    execute_render_command(fixture.renderer(), textured);
    fixture.read_pixels();
    const Color texture_last = fixture.pixel_color(510, 110);
    require(texture_last.b > texture_last.r,
        "A texture submitted after a primitive must remain on top");

    fixture.clear();
    execute_render_command(fixture.renderer(), textured);
    execute_render_command(fixture.renderer(), primitive);
    fixture.read_pixels();
    const Color primitive_last = fixture.pixel_color(510, 110);
    require(primitive_last.r > primitive_last.b,
        "A primitive submitted after a texture must remain on top");
    SDL_DestroyTexture(texture);
}

void test_invalid_world_primitive_geometry_is_skipped()
{
    using namespace elysia::core;
    SdlStrokeFixture fixture(1280,720);

    ScreenRenderCommand rect;
    rect.type = RenderCommandType::FillRect;
    rect.screen_rect = {
        std::numeric_limits<float>::quiet_NaN(), 20, 10, 10};
    rect.color = k_white;
    execute_render_command(fixture.renderer(), rect);

    ScreenRenderCommand circle;
    circle.type = RenderCommandType::FillCircle;
    circle.circle_center = {50, 50};
    circle.circle_radius = 0.0f;
    circle.color = k_white;
    execute_render_command(fixture.renderer(), circle);

    ScreenRenderCommand line;
    line.type = RenderCommandType::DrawLine;
    line.line_start = {80, 80};
    line.line_end = line.line_start;
    line.color = k_white;
    execute_render_command(fixture.renderer(), line);

    ScreenRenderCommand non_finite_triangle;
    non_finite_triangle.type = RenderCommandType::FillTriangle;
    non_finite_triangle.triangle_vertices = {
        Vector2{100, 100},
        Vector2{std::numeric_limits<float>::quiet_NaN(), 110},
        Vector2{110, 120}
    };
    non_finite_triangle.color = k_white;
    execute_render_command(fixture.renderer(),non_finite_triangle);

    ScreenRenderCommand infinite_triangle;
    infinite_triangle.type = RenderCommandType::FillTriangle;
    infinite_triangle.triangle_vertices = {
        Vector2{140, 100},
        Vector2{150, std::numeric_limits<float>::infinity()},
        Vector2{160, 120}
    };
    infinite_triangle.color = k_white;
    execute_render_command(fixture.renderer(),infinite_triangle);

    ScreenRenderCommand repeated_triangle;
    repeated_triangle.type = RenderCommandType::FillTriangle;
    repeated_triangle.triangle_vertices = {
        Vector2{180, 100},Vector2{180, 100},Vector2{190, 120}
    };
    repeated_triangle.color = k_white;
    execute_render_command(fixture.renderer(),repeated_triangle);

    ScreenRenderCommand collinear_triangle;
    collinear_triangle.type = RenderCommandType::FillTriangle;
    collinear_triangle.triangle_vertices = {
        Vector2{220, 100},Vector2{230, 110},Vector2{240, 120}
    };
    collinear_triangle.color = k_white;
    execute_render_command(fixture.renderer(),collinear_triangle);
    fixture.read_pixels();

    require(!fixture.visible(50, 50)
            && !fixture.visible(80, 80)
            && !fixture.visible(105, 110)
            && !fixture.visible(150, 110)
            && !fixture.visible(185, 110)
            && !fixture.visible(230, 110),
        "Invalid primitives and triangles must not emit pixels");
}
}

int main()
{
    for (const auto output : {
             elysia::core::Vector2{ 960,540 },
             elysia::core::Vector2{ 1280,720 },
             elysia::core::Vector2{ 1600,900 },
             elysia::core::Vector2{ 1000,700 } })
    {
        require_hairline_rect(
            static_cast<int>(output.x),
            static_cast<int>(output.y));
        require_curved_and_line_strokes(
            static_cast<int>(output.x),
            static_cast<int>(output.y));
    }
    test_logical_stroke_scales_and_renderer_state_is_restored();
    test_world_primitives_render_and_restore_state();
    test_world_triangles_render_and_restore_state();
    test_texture_and_world_primitive_submission_order();
    test_invalid_world_primitive_geometry_is_skipped();
    return EXIT_SUCCESS;
}
