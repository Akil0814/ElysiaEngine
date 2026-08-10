#include "controller_manager.h"

#include "../tools/logger.h"

#include <algorithm>

namespace elysia::input
{
ControllerManager::~ControllerManager()
{
    shutdown();
}

void ControllerManager::init()
{
    if (_is_initialized)
    {
        return;
    }

    _is_initialized = true;
    open_connected_controllers();
}

void ControllerManager::shutdown()
{
    if (!_is_initialized)
    {
        return;
    }

    close_all_controllers();
    _is_initialized = false;
}

void ControllerManager::handle_event(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_CONTROLLERDEVICEADDED:
        open_controller(event.cdevice.which);
        break;

    case SDL_CONTROLLERDEVICEREMOVED:
        close_controller(event.cdevice.which);
        break;

    default:
        break;
    }
}

void ControllerManager::open_connected_controllers()
{
    const int joystick_count = SDL_NumJoysticks();
    for (int joystick_index = 0; joystick_index < joystick_count; ++joystick_index)
    {
        open_controller(joystick_index);
    }
}

void ControllerManager::open_controller(int joystick_index)
{
    if (!SDL_IsGameController(joystick_index))
    {
        return;
    }

    SDL_GameController* controller = SDL_GameControllerOpen(joystick_index);
    if (!controller)
    {
        elysia::tools::Logger::instance()->warn("input","Failed to open controller");
        elysia::tools::Logger::instance()->warn("input",SDL_GetError());
        SDL_ClearError();
        return;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    const SDL_JoystickID joystick_id = SDL_JoystickInstanceID(joystick);

    for (SDL_GameController* existing_controller : _controllers)
    {
        if (!existing_controller)
        {
            continue;
        }

        SDL_Joystick* existing_joystick = SDL_GameControllerGetJoystick(existing_controller);
        if (SDL_JoystickInstanceID(existing_joystick) == joystick_id)
        {
            SDL_GameControllerClose(controller);
            return;
        }
    }

    _controllers.push_back(controller);
}

void ControllerManager::close_controller(SDL_JoystickID joystick_id)
{
    std::vector<SDL_GameController*>::iterator iter = std::remove_if(
        _controllers.begin(),
        _controllers.end(),
        [joystick_id](SDL_GameController* controller)
        {
            if (!controller)
            {
                return true;
            }

            SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
            if (SDL_JoystickInstanceID(joystick) != joystick_id)
            {
                return false;
            }

            SDL_GameControllerClose(controller);
            return true;
        }
    );

    _controllers.erase(iter, _controllers.end());
}

void ControllerManager::close_all_controllers()
{
    for (SDL_GameController* controller : _controllers)
    {
        if (controller)
        {
            SDL_GameControllerClose(controller);
        }
    }

    _controllers.clear();
}

}
