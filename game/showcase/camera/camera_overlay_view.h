#pragma once
#include "engine/ui/core/ui_element.h"
#include <functional>
#include <optional>
#include <limits>
namespace example::showcase::camera {
struct CameraOverlayData {
    elysia::core::Vector2 viewport;
    std::optional<elysia::core::Rect> focus,primary,bounds;
};
class CameraOverlayView final : public elysia::ui::UiElement {
public:
    explicit CameraOverlayView(std::function<CameraOverlayData()> data)
        :UiElement(elysia::core::Rect::zero(),std::numeric_limits<int>::max()),_data(std::move(data)) {}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& commands) const override;
private: std::function<CameraOverlayData()> _data;
};
}
