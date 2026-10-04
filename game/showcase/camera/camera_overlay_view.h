#pragma once
#include "camera_demo_state.h"
#include "engine/camera/camera.h"
#include "engine/ui/core/ui_element.h"
#include <functional>
#include <vector>
namespace example::showcase::camera {
struct CameraOverlayData {
    elysia::camera::Camera camera;
    FollowMode mode = FollowMode::MultiTarget;
    bool dead_zone = true;
    std::optional<elysia::core::Rect> focus,primary,bounds;
    std::vector<elysia::core::Vector2> nodes;
};
class CameraOverlayView final : public elysia::ui::UiElement {
public:
    explicit CameraOverlayView(std::function<CameraOverlayData()> data)
        :UiElement(elysia::core::Rect::zero(),-1),_data(std::move(data)) {}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>&) const override;
private:std::function<CameraOverlayData()> _data;
};
}
