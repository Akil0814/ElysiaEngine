#include "camera_overlay_view.h"
#include "engine/ui/widgets/label/ui_label.h"
#include <cmath>
namespace example::showcase::camera {
void CameraOverlayView::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& commands) const {
    using namespace elysia::core;
    const auto data=_data();const auto viewport=data.camera.viewport_size();
    const Rect content{0,324,viewport.x,std::max(0.0f,viewport.y-408)};
    commands.push_back(make_ui_fill_rect_command({0,0,viewport.x,324},{20,24,32}));
    commands.push_back(make_ui_fill_rect_command({0,viewport.y-84,viewport.x,84},{20,24,32}));
    const auto outline=[&](Rect rect,Color color){auto command=make_ui_draw_rect_command(rect,color);set_ui_command_clip_rect(command,content);commands.push_back(command);};
    const auto line=[&](Vector2 from,Vector2 to,Color color){auto command=make_ui_draw_line_command(from,to,color);set_ui_command_clip_rect(command,content);commands.push_back(command);};
    const auto view=data.camera.view_rect();
    for(float x=std::floor(view.left()/200)*200;x<=view.right();x+=200)
        line(data.camera.world_to_screen(Vector2{x,view.top()}),data.camera.world_to_screen(Vector2{x,view.bottom()}),x==0?Color{110,120,140}:Color{42,49,62});
    for(float y=std::floor(view.top()/200)*200;y<=view.bottom();y+=200)
        line(data.camera.world_to_screen(Vector2{view.left(),y}),data.camera.world_to_screen(Vector2{view.right(),y}),y==0?Color{110,120,140}:Color{42,49,62});
    if(data.mode==FollowMode::MultiTarget && data.dead_zone) {
        outline(Rect::from_center(viewport*.5f,viewport*.70f),{65,180,255});
        outline(Rect::from_center(viewport*.5f,viewport*.55f),{100,210,130});
    } else if(data.mode==FollowMode::DeadZone)outline(Rect::from_center(viewport*.5f,viewport*.4f),{100,210,130});
    if(data.focus)outline(*data.focus,{180,180,180});
    if(data.primary)outline(Rect::from_center(data.primary->center(),data.primary->size()+Vector2{8,8}),{255,255,255});
    if(data.bounds)outline(*data.bounds,{240,90,90});
    for(const auto point:{Vector2{-400,-240},Vector2{0,-160},Vector2{400,240}})
        outline(data.camera.world_to_screen(Rect::from_center(point,{50,50})),{145,105,190});
    for(std::size_t i=0;i<data.nodes.size();++i) {
        if(i)line(data.nodes[i-1],data.nodes[i],{240,200,80});
        outline(Rect::from_center(data.nodes[i],{12,12}),{240,200,80});
        bool duplicate=false;
        for(std::size_t j=0;j<i;++j)if(data.nodes[j]==data.nodes[i])duplicate=true;
        if(duplicate)continue;
        std::string indices=std::to_string(i+1);
        for(std::size_t j=i+1;j<data.nodes.size();++j)
            if(data.nodes[j]==data.nodes[i])indices+=" / "+std::to_string(j+1);
        elysia::ui::UiLabel label(Rect{data.nodes[i].x+8,data.nodes[i].y-22,70,22},0,elysia::ui::ui_raw_text(std::move(indices)));
        const auto first=commands.size();label.submit_ui_render_commands(commands);
        for(auto j=first;j<commands.size();++j)set_ui_command_clip_rect(commands[j],content);
    }
}
}
