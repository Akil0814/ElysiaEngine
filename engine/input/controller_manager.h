#pragma once

#include <SDL.h>

#include <vector>

namespace elysia::input
{
class ControllerManager
{
public:
    ControllerManager() = default;
    ~ControllerManager();

    ControllerManager(const ControllerManager&) = delete;
    ControllerManager& operator=(const ControllerManager&) = delete;

    ControllerManager(ControllerManager&&) = delete;
    ControllerManager& operator=(ControllerManager&&) = delete;

    void init();
    void shutdown();
    void handle_event(const SDL_Event& event);

private:
    void open_connected_controllers();
    void open_controller(int joystick_index);
    void close_controller(SDL_JoystickID joystick_id);
    void close_all_controllers();

private:
    std::vector<SDL_GameController*> _controllers;
    bool _is_initialized = false;
};

}
