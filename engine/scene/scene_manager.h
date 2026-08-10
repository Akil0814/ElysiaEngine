#pragma once

#include <SDL.h>

#include <functional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "scene.h"
#include "detail/scene_factory.h"
#include "scene_manager_observer.h"
#include "routing/scene_request.h"
#include "routing/scene_request_observer.h"

#include "../core/event/subject.h"

namespace elysia::scene
{
class SceneManager
    : public elysia::core::Subject<SceneManagerObserver>
    , public SceneRequestObserver
{
public:
    SceneManager() = default;
    ~SceneManager();
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;

    template <typename T>
    void register_game_scene(SceneKey scene_key);

    template <typename T>
    void register_engine_scene(SceneKey scene_key);

    void set_runtime_context(const SceneRuntimeContext& context) noexcept;

    void start(const SceneRoute& route);

    void on_input(
        const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events
    );

    void on_update(double delta);
    void on_render(SDL_Renderer* renderer);

    void on_scene_request(const SceneRequest& request) override;

    [[nodiscard]] SceneKey current_scene_key() const noexcept
    {
        return _current_scene_key;
    }

    void shutdown();

private:
    using SceneProvider = std::function<Scene*(SceneReloadMode reload_mode)>;

    template <typename T>
    void add_scene_provider(SceneKey scene_key);

    void notify_quit_requested();
    void process_pending_request();

    void switch_to_registered_scene(const SceneRoute& route);

    void switch_to_scene(
        Scene* next_scene,
        const SceneRoute& route
    );

    void attach_to_scene(Scene* scene);
    void detach_from_scene(Scene* scene);
    [[noreturn]] static void throw_invalid_route_key(SceneKey key);

private:
    Scene* _current_scene = nullptr;
    SceneKey _current_scene_key = SceneKeys::Invalid;

    SceneFactory _scene_factory;
    std::unordered_map<SceneKey, SceneProvider> _scene_providers;
    const SceneRuntimeContext* _runtime_context = nullptr;

    SceneRequest _pending_request{};
    bool _has_pending_request = false;
    bool _is_processing_request = false;
};

template <typename T>
void SceneManager::register_game_scene(SceneKey scene_key)
{
    if (!SceneKeys::is_game(scene_key))
        throw std::logic_error("SceneManager::register_game_scene received a SceneKey outside the game range [1, 999].");

    add_scene_provider<T>(scene_key);
}

template <typename T>
void SceneManager::register_engine_scene(SceneKey scene_key)
{
    if (!SceneKeys::is_engine(scene_key) && !SceneKeys::is_easter_egg(scene_key))
        throw std::logic_error("SceneManager::register_engine_scene received a SceneKey outside the engine-owned keys.");

    add_scene_provider<T>(scene_key);
}

template <typename T>
void SceneManager::add_scene_provider(SceneKey scene_key)
{
    static_assert(
        std::is_base_of_v<Scene, T>,
        "T must derive from Scene."
    );

    if (_scene_providers.find(scene_key) != _scene_providers.end())
        throw std::logic_error("SceneManager scene registration received a duplicate SceneKey.");

    _scene_providers.emplace(
        scene_key,
        [this](SceneReloadMode reload_mode) -> Scene*
        {
            T* existing_scene = _scene_factory.try_find_scene<T>();

            if (reload_mode == SceneReloadMode::Recreate)
            {
                if (existing_scene)
                {
                    if (_current_scene == existing_scene)
                    {
                        detach_from_scene(_current_scene);
                        _current_scene->on_exit();
                        _current_scene->clear_runtime_context();
                        _current_scene = nullptr;
                        _current_scene_key = SceneKeys::Invalid;
                    }
                    else
                    {
                        detach_from_scene(existing_scene);
                        existing_scene->clear_runtime_context();
                    }

                    _scene_factory.destroy_scene<T>();
                }
            }

            return _scene_factory.get_scene<T>();
        }
    );
}

}
