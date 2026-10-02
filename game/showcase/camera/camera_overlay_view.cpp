#include "game/showcase/camera/camera_overlay_view.h"
namespace example::showcase::camera {
void CameraOverlayView::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& commands) const
{
    using namespace elysia::core;
    const auto data=_data();
    const Rect content{0,172,data.viewport.x,std::max(0.0f,data.viewport.y-260)};
    const auto outline=[&](Rect rect,Color color){
        auto command=make_ui_draw_rect_command(rect,color);
        set_ui_command_clip_rect(command,content);
        commands.push_back(command);
    };
    outline(Rect::from_center(data.viewport*.5f,data.viewport*.70f),{65,180,255});
    outline(Rect::from_center(data.viewport*.5f,data.viewport*.55f),{100,210,130});
    if(data.focus)outline(*data.focus,{180,180,180});
    if(data.primary)outline(Rect::from_center(data.primary->center(),data.primary->size()+Vector2{8,8}),{255,255,255});
    if(data.bounds)outline(*data.bounds,{240,90,90});
}
}
