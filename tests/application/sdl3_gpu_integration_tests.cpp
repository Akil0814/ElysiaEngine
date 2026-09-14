#include "engine/application/presentation/application_sdl_presentation.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/input/input_system.h"
#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <filesystem>
#include <iostream>

int main()
{
    using elysia::tests::require;
    using namespace elysia::core;
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD),SDL_GetError());
    SDL_Window* window = SDL_CreateWindow("Elysia SDL3 GPU verification",640,360,SDL_WINDOW_HIDDEN);
    require(window != nullptr,SDL_GetError());
    SDL_SetHint(SDL_HINT_GPU_DRIVER,"elysia-invalid-driver");
    SDL_Renderer* failed = SDL_CreateGPURenderer(nullptr,window);
    require(failed == nullptr,"an unavailable GPU backend must fail without silently selecting another renderer");
    SDL_ResetHint(SDL_HINT_GPU_DRIVER);
    SDL_Renderer* renderer = SDL_CreateGPURenderer(nullptr,window);
    require(renderer != nullptr,SDL_GetError());
    std::cout << "GPU backend: " << SDL_GetGPUDeviceDriver(SDL_GetGPURendererDevice(renderer)) << '\n';
    require(elysia::application::detail::configure_sdl_renderer_presentation(renderer,640,360).has_value(),"GPU presentation");
    require(elysia::application::detail::configure_sdl_texture_filter(renderer,{}).has_value(),"GPU texture filtering");

    SDL_SetRenderDrawColor(renderer,20,24,32,255);
    SDL_RenderClear(renderer);
    UiRenderCommand circle;
    circle.type=UiRenderCommandType::FillCircle;
    circle.circle_center={100,100}; circle.circle_radius=45; circle.color={220,70,90,255};
    execute_render_command(renderer,circle);
    UiRenderCommand rounded;
    rounded.type=UiRenderCommandType::FillRoundedRect;
    rounded.screen_rect=Rect({180,55},{180,90}); rounded.corner_radius=20; rounded.color={50,190,130,128};
    execute_render_command(renderer,rounded);
    UiRenderCommand stroke=rounded;
    stroke.type=UiRenderCommandType::DrawRoundedRect; stroke.color={230,230,240,255};
    stroke.stroke_width={UiStrokeWidthMode::Logical,3};
    execute_render_command(renderer,stroke);
    const std::filesystem::path root=ELYSIA_SOURCE_DIR;
    SDL_Surface* source=SDL_CreateSurface(24,24,SDL_PIXELFORMAT_RGBA32);
    require(source != nullptr,"GPU test surface");
    SDL_FillSurfaceRect(source,nullptr,SDL_MapSurfaceRGBA(source,80,140,240,255));
    SDL_Rect left_half{0,0,12,24};
    SDL_FillSurfaceRect(source,&left_half,SDL_MapSurfaceRGBA(source,240,80,60,255));
    SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,source);
    SDL_DestroySurface(source);
    require(texture != nullptr,"GPU texture upload");
    execute_textured_render_command(renderer,texture,Rect({410,60},{80,80}),200,std::nullopt,false,{},30,{0.5f,0.5f},SpriteFlip::Horizontal);
    execute_textured_render_command(renderer,texture,Rect({50,280},{80,40}),255,std::nullopt,false,{},0,{0.5f,0.5f},SpriteFlip::Horizontal);

    require(TTF_Init(),"GPU font initialization");
    const auto font_path=root/"assets/engine/fonts/NotoSans-Regular.ttf";
    TTF_Font* font=TTF_OpenFont(font_path.string().c_str(),24);
    require(font != nullptr,"GPU font load");
    SDL_Surface* text=TTF_RenderText_Blended(font,"SDL3 GPU / Elysia",0,{240,240,245,255});
    require(text != nullptr,"GPU glyph rasterization");
    SDL_Texture* label=SDL_CreateTextureFromSurface(renderer,text);
    SDL_FRect label_rect{50,210,static_cast<float>(text->w),static_cast<float>(text->h)};
    SDL_RenderTexture(renderer,label,nullptr,&label_rect);
    SDL_DestroySurface(text);
    SDL_Rect clip{500,260,40,40};
    require(SDL_SetRenderClipRect(renderer,&clip),"GPU clip setup");
    SDL_SetRenderDrawColor(renderer,240,180,30,255);
    SDL_FRect clipped_fill{480,240,100,80};
    require(SDL_RenderFillRect(renderer,&clipped_fill),"GPU clipped fill");
    require(SDL_SetRenderClipRect(renderer,nullptr),"GPU clip reset");
    SDL_Surface* capture=SDL_RenderReadPixels(renderer,nullptr);
    require(capture != nullptr,"GPU pixel readback");
    Uint8 r=0,g=0,b=0,a=0;
    require(SDL_ReadSurfacePixel(capture,100,100,&r,&g,&b,&a),"GPU pixel sample");
    require(r>200 && g<100 && b<120,"gfx circle must render with the expected color on GPU");
    require(SDL_ReadSurfacePixel(capture,260,100,&r,&g,&b,&a),"rounded alpha sample");
    require(std::abs(int(r)-35)<=1 && std::abs(int(g)-107)<=1 && std::abs(int(b)-81)<=1,
        "rounded-box alpha must blend once against the background (UNORM rounding within one byte)");
    require(SDL_ReadSurfacePixel(capture,510,270,&r,&g,&b,&a) && r==240 && g==180 && b==30,"GPU clip interior");
    require(SDL_ReadSurfacePixel(capture,490,270,&r,&g,&b,&a) && r==20 && g==24 && b==32,"GPU clip must exclude outside pixels");
    require(SDL_ReadSurfacePixel(capture,60,300,&r,&g,&b,&a) && r==80 && g==140 && b==240,"horizontal flip must move the blue half to the left");
    require(SDL_ReadSurfacePixel(capture,120,300,&r,&g,&b,&a) && r==240 && g==80 && b==60,"horizontal flip must move the red half to the right");
    require(IMG_SavePNG(capture,"sdl3-gpu-verification.png"),"save GPU verification artifact");
    SDL_DestroySurface(capture);
    require(SDL_RenderPresent(renderer),"GPU present");
    require(SDL_SetWindowSize(window,800,600) && SDL_SyncWindow(window),"GPU window resize");
    float window_x=0,window_y=0,logical_x=0,logical_y=0;
    require(SDL_RenderCoordinatesToWindow(renderer,320,180,&window_x,&window_y),"GPU logical to window coordinates");
    require(SDL_RenderCoordinatesFromWindow(renderer,window_x,window_y,&logical_x,&logical_y),"GPU window to logical coordinates");
    require(std::abs(logical_x-320)<0.01f && std::abs(logical_y-180)<0.01f,"GPU resized coordinate conversion must round trip");
    require(SDL_SetWindowFullscreen(window,true) && SDL_SyncWindow(window),"GPU fullscreen entry");
    require(SDL_SetWindowFullscreen(window,false) && SDL_SyncWindow(window),"GPU fullscreen exit");
    SDL_DestroyTexture(label); TTF_CloseFont(font); TTF_Quit();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
}
