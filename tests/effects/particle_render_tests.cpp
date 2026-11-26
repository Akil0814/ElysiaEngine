#define SDL_MAIN_HANDLED
#include "particle_test_fixture.h"
#include "engine/core/render/sdl_render_boundary.h"
#include <cmath>
#include <string_view>
#include <filesystem>
#include <iostream>

using namespace elysia;
using namespace elysia::effects;
using tests::require;
namespace
{
int batches=0;
bool count_batch(std::string_view operation) { if(operation=="SDL_RenderGeometry") ++batches; return true; }
core::Color pixel(SDL_Surface* image,int x,int y)
{
    core::Color c; require(SDL_ReadSurfacePixel(image,x,y,&c.r,&c.g,&c.b,&c.a),SDL_GetError()); return c;
}
void render_tests(tests::ParticleFixture& f)
{
    camera::Camera camera({0,0},{128,96}); auto d=f.definition();
    ParticleEffect red(d,f.texture,{8,4},{0,0},1);
    f.clear(); require(SDL_SetTextureBlendMode(f.texture,SDL_BLENDMODE_NONE),"set borrowed texture mode");
    require(SDL_SetTextureAlphaMod(f.texture,17) && SDL_SetTextureColorMod(f.texture,20,30,40), "set borrowed texture modulation");
    require(red.render(f.renderer,camera),"textured batch");
    Uint8 alpha=0, r=0, g=0, b=0;
    require(SDL_GetTextureAlphaMod(f.texture,&alpha) && alpha==17
        && SDL_GetTextureColorMod(f.texture,&r,&g,&b) && r==20 && g==30 && b==40,
        "geometry leaves shared color and alpha modulation unchanged");
    auto* pixels=SDL_RenderReadPixels(f.renderer,nullptr); require(pixels,SDL_GetError());
    auto center=pixel(pixels,64,48); require(center.r>240 && center.b<10,"UV selects left texture region");
    require(pixel(pixels,64,38).r==0,"quad dimensions are respected"); SDL_DestroySurface(pixels);
    SDL_BlendMode mode; require(SDL_GetTextureBlendMode(f.texture,&mode) && mode==SDL_BLENDMODE_NONE,"texture blend restored");
    d.angle_degrees={90,90}; d.start_color.a=d.end_color.a=128;
    ParticleEffect rotated(d,f.texture,{8,4},{0,0},1);
    f.clear(); require(rotated.render(f.renderer,camera),"rotated alpha batch"); pixels=SDL_RenderReadPixels(f.renderer,nullptr); require(pixels,"read rotated pixels");
    require(std::abs(int(pixel(pixels,64,40).r)-128)<=2 && pixel(pixels,54,48).r==0,"rotation and vertex alpha blend correctly"); SDL_DestroySurface(pixels);
    d=f.definition(); d.start_color={255,255,255,255}; d.end_color={255,255,255,0}; d.end_size={12,4};
    ParticleEffect fading(d,f.texture,{8,4},{0,0},1); fading.update(1);
    f.clear(); require(fading.render(f.renderer,camera),"age-dependent size and alpha"); pixels=SDL_RenderReadPixels(f.renderer,nullptr); require(pixels,"fade capture");
    require(std::abs(int(pixel(pixels,64,48).r)-128)<=2 && pixel(pixels,74,48).r==0,"lifetime interpolation changes size and alpha"); SDL_DestroySurface(pixels);
    camera.set_center({10,0}); camera.set_zoom(2);
    f.clear(); require(red.render(f.renderer,camera),"camera projection"); pixels=SDL_RenderReadPixels(f.renderer,nullptr); require(pixels,"camera capture");
    require(pixel(pixels,44,48).r>240 && pixel(pixels,90,48).r==0,"camera center and zoom project the batch"); SDL_DestroySurface(pixels);
    camera.set_center({0,0});camera.set_zoom(1);
    ParticleRuntime runtime; int scene=0; d=f.definition(); d.start_color.a=d.end_color.a=128;
    auto first=runtime.create(d,f.texture,{.effect_key=d.effect_key},&scene); require(first,"first batch");
    d.source_rect=core::Rect{4,0,4,4}; auto second=runtime.create(d,f.texture,{.effect_key=d.effect_key},&scene); require(second,"second batch");
    require(runtime.cancel(*first),"retire first slot");d.source_rect=core::Rect{0,0,4,4};auto third=runtime.create(d,f.texture,{.effect_key=d.effect_key},&scene);require(third,"reused batch slot");
    f.clear();batches=0;core::detail::render_operation_probe=count_batch;
    require(runtime.render(f.renderer,core::DepthLayer::EffectFront,camera),"ordered transparent batches");core::detail::render_operation_probe=nullptr;
    require(batches==2,"one geometry submission per visible group, not per particle");pixels=SDL_RenderReadPixels(f.renderer,nullptr);require(pixels,"order capture");
    center=pixel(pixels,64,48);require(std::abs(int(center.r)-128)<=2 && std::abs(int(center.b)-64)<=2,"creation order survives slot reuse");SDL_DestroySurface(pixels);
    runtime.clear(); d.burst_count=12;
    require(runtime.create(d,f.texture,{.effect_key=d.effect_key},&scene), "large first group");
    require(runtime.create(d,f.texture,{.effect_key=d.effect_key},&scene), "large second group");
    batches=0;core::detail::render_operation_probe=count_batch;
    require(runtime.render(f.renderer,core::DepthLayer::EffectFront,camera), "large group batch render");
    core::detail::render_operation_probe=nullptr;
    require(batches==2 && runtime.particle_count()==24, "batch count grows with groups, not individual particles");
    core::detail::render_operation_probe=[](std::string_view op){return op!="SDL_RenderGeometry";};
    auto failure=red.render(f.renderer,camera);core::detail::render_operation_probe=nullptr;
    require(!failure && failure.error().operation=="SDL_RenderGeometry","backend failure propagates through render boundary");
    require(SDL_GetTextureBlendMode(f.texture,&mode) && mode==SDL_BLENDMODE_NONE,"failure restores borrowed blend state");
    d=f.definition();d.blend=ParticleBlendMode::Add;ParticleEffect additive(d,f.texture,{8,4},{0,0},1);
    f.clear();require(additive.render(f.renderer,camera),"additive batch");require(additive.render(f.renderer,camera),"second additive batch");
    require(SDL_GetTextureBlendMode(f.texture,&mode) && mode==SDL_BLENDMODE_NONE,"additive rendering restores blend state");
    f.clear();require(red.render(f.renderer,camera),"verification image");pixels=SDL_RenderReadPixels(f.renderer,nullptr);require(pixels,"verification capture");
    require(IMG_SavePNG(pixels,"particle-batch-verification.png"),"save verification image");SDL_DestroySurface(pixels);
}
}
int main(int argc,char** argv)
{
    const bool gpu=argc>1 && std::string_view(argv[1])=="--gpu";
    tests::ParticleFixture fixture(gpu);
    if(gpu) std::cout << "GPU backend: " << SDL_GetGPUDeviceDriver(SDL_GetGPURendererDevice(fixture.renderer)) << '\n';
    render_tests(fixture);
}
