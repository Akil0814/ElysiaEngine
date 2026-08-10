#define SDL_MAIN_HANDLED

#include "engine/camera/camera_manager.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <utility>

namespace
{
using elysia::tests::require;
using elysia::camera::CameraManager;
using elysia::camera::CameraSlot;
using elysia::core::Rect;
using elysia::core::Vector2;

constexpr std::array<CameraSlot, 4> k_camera_slots{
    CameraSlot::Main,
    CameraSlot::Cinematic,
    CameraSlot::Auxiliary1,
    CameraSlot::Auxiliary2
};

using CameraAccess = decltype(
    std::declval<CameraManager&>().camera(CameraSlot::Main)
);
static_assert(std::is_same_v<CameraAccess, const elysia::camera::Camera&>);

void reset_cameras()
{
    auto* manager = CameraManager::instance();
    manager->reset_all();

    for (CameraSlot slot : k_camera_slots)
    {
        manager->set_viewport_size(slot, Vector2::zero());
    }
}

void test_fixed_slots_are_independent()
{
    reset_cameras();
    auto* manager = CameraManager::instance();

    manager->set_center(CameraSlot::Main, Vector2(10.0f, 20.0f));
    manager->set_center(CameraSlot::Cinematic, Vector2(30.0f, 40.0f));
    manager->set_center(CameraSlot::Auxiliary1, Vector2(50.0f, 60.0f));
    manager->set_center(CameraSlot::Auxiliary2, Vector2(70.0f, 80.0f));
    manager->set_zoom(CameraSlot::Main, 2.0f);
    manager->set_zoom(CameraSlot::Cinematic, 3.0f);

    require(manager->camera(CameraSlot::Main).center() == Vector2(10.0f, 20.0f),
        "Main camera must retain its own center");
    require(manager->camera(CameraSlot::Cinematic).center() == Vector2(30.0f, 40.0f),
        "Cinematic camera must retain its own center");
    require(manager->camera(CameraSlot::Auxiliary1).center() == Vector2(50.0f, 60.0f),
        "Auxiliary1 camera must retain its own center");
    require(manager->camera(CameraSlot::Auxiliary2).center() == Vector2(70.0f, 80.0f),
        "Auxiliary2 camera must retain its own center");
    require(manager->camera(CameraSlot::Main).zoom() == 2.0f,
        "Main camera must retain its own zoom");
    require(manager->camera(CameraSlot::Cinematic).zoom() == 3.0f,
        "Cinematic camera must retain its own zoom");
    require(manager->camera(CameraSlot::Auxiliary1).zoom() == 1.0f,
        "zoom changes must not affect other slots");

    manager->set_focus_rect(CameraSlot::Cinematic, Rect(90.0f, 40.0f, 20.0f, 20.0f));
    manager->set_follow_strategy(
        CameraSlot::Cinematic,
        std::make_unique<elysia::camera::HardFollowStrategy>()
    );
    manager->update(0.0);

    require(manager->camera(CameraSlot::Cinematic).center() == Vector2(100.0f, 50.0f),
        "CameraManager::update must advance configured cameras");
    require(manager->camera(CameraSlot::Main).center() == Vector2(10.0f, 20.0f),
        "updating Cinematic must not change Main");
}

void test_requests_are_fifo_and_targeted()
{
    reset_cameras();
    auto* manager = CameraManager::instance();
    manager->set_center(CameraSlot::Main, Vector2(5.0f, 5.0f));

    const elysia::camera::CameraShakeParams shake{
        .amplitude = Vector2(0.0f, 10.0f),
        .duration_seconds = 1.0,
        .frequency_hz = 0.0
    };

    manager->request_shake(CameraSlot::Main, shake);
    manager->request_clear_effects(CameraSlot::Main);
    manager->update(0.1);
    require(manager->camera(CameraSlot::Main).center() == Vector2(5.0f, 5.0f),
        "a later clear request must cancel an earlier shake before camera update");

    manager->request_clear_effects(CameraSlot::Main);
    manager->request_shake(CameraSlot::Main, shake);
    manager->update(0.1);
    require(manager->camera(CameraSlot::Main).center().nearly_equals(Vector2(5.0f, 14.0f)),
        "a later shake request must remain active after an earlier clear request");
    require(manager->camera(CameraSlot::Auxiliary1).center() == Vector2::zero(),
        "Main requests must not affect Auxiliary1");

    manager->set_zoom(CameraSlot::Main, 1.0f);
    manager->request_zoom_to(CameraSlot::Main, 3.0f, 2.0);
    manager->request_clear_effects(CameraSlot::Main);
    manager->update(1.0);
    require(manager->camera(CameraSlot::Main).zoom() == 1.0f,
        "a later clear request must cancel an earlier zoom transition");

    manager->request_clear_effects(CameraSlot::Main);
    manager->request_zoom_to(CameraSlot::Main, 3.0f, 2.0);
    manager->update(1.0);
    require(manager->camera(CameraSlot::Main).zoom() == 2.0f,
        "a later zoom request must remain active after an earlier clear request");

    manager->request_zoom_to(CameraSlot::Main, 5.0f, 1.0);
    manager->set_zoom(CameraSlot::Main, 1.5f);
    manager->update(1.0);
    require(manager->camera(CameraSlot::Main).zoom() == 1.5f,
        "immediate zoom must cancel queued transitions for the same slot");
}

void test_reset_preserves_viewport_and_other_slots()
{
    reset_cameras();
    auto* manager = CameraManager::instance();

    manager->set_viewport_size(CameraSlot::Main, Vector2(320.0f, 180.0f));
    manager->set_center(CameraSlot::Main, Vector2(25.0f, 30.0f));
    manager->set_zoom(CameraSlot::Main, 2.0f);
    manager->set_center(CameraSlot::Auxiliary1, Vector2(75.0f, 80.0f));
    manager->set_zoom(CameraSlot::Auxiliary1, 3.0f);
    manager->request_shake(CameraSlot::Main, elysia::camera::CameraShakeParams{});

    manager->reset(CameraSlot::Main);
    manager->update(0.01);

    require(manager->camera(CameraSlot::Main).center() == Vector2::zero(),
        "reset must clear the camera center and pending requests");
    require(manager->camera(CameraSlot::Main).viewport_size() == Vector2(320.0f, 180.0f),
        "reset must preserve the camera viewport");
    require(manager->camera(CameraSlot::Main).zoom() == 1.0f,
        "reset must restore the default zoom");
    require(manager->camera(CameraSlot::Auxiliary1).center() == Vector2(75.0f, 80.0f),
        "resetting Main must not reset Auxiliary1");
    require(manager->camera(CameraSlot::Auxiliary1).zoom() == 3.0f,
        "resetting Main must not reset Auxiliary1 zoom");
}

class CameraScene final : public elysia::scene::Scene
{
public:
    CameraScene()
    {
        last_instance = this;
    }

    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void reset() override {}

    void select_camera(CameraSlot slot) noexcept
    {
        set_render_camera_slot(slot);
    }

    void request_self_reset()
    {
        request_scene_switch(1, {}, elysia::scene::SceneReloadMode::Reset);
    }

    void request_self_reuse()
    {
        request_scene_switch(1, {}, elysia::scene::SceneReloadMode::Reuse);
    }

    static inline CameraScene* last_instance = nullptr;
};

void test_scene_defaults_and_main_lifecycle()
{
    reset_cameras();
    auto* cameras = CameraManager::instance();
    cameras->set_viewport_size(CameraSlot::Main, Vector2(640.0f, 360.0f));
    cameras->set_center(CameraSlot::Main, Vector2(10.0f, 20.0f));
    cameras->set_center(CameraSlot::Cinematic, Vector2(70.0f, 80.0f));

    {
        CameraScene scene;
        require(scene.render_camera_slot() == CameraSlot::Main,
            "Scene must default to Main camera");
        require(scene.camera().center() == Vector2(10.0f, 20.0f),
            "Scene camera access must read Main by default");

        scene.select_camera(CameraSlot::Cinematic);
        require(scene.camera().center() == Vector2(70.0f, 80.0f),
            "Scene must read an explicitly selected render camera");
    }

    elysia::scene::SceneManager scene_manager;
    scene_manager.register_game_scene<CameraScene>(1);
    scene_manager.start(elysia::scene::SceneRoute{
        .target = 1,
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    });

    require(cameras->camera(CameraSlot::Main).center() == Vector2::zero(),
        "entering the first managed scene must reset Main");
    require(cameras->camera(CameraSlot::Main).viewport_size() == Vector2(640.0f, 360.0f),
        "scene transitions must preserve Main viewport");
    require(cameras->camera(CameraSlot::Cinematic).center() == Vector2(70.0f, 80.0f),
        "scene transitions must preserve Cinematic");

    cameras->set_center(CameraSlot::Main, Vector2(35.0f, 45.0f));
    CameraScene::last_instance->request_self_reset();
    scene_manager.on_update(0.0);

    require(cameras->camera(CameraSlot::Main).center() == Vector2::zero(),
        "Reset reload must clear Main before re-entering the scene");
    require(cameras->camera(CameraSlot::Cinematic).center() == Vector2(70.0f, 80.0f),
        "Reset reload must not clear Cinematic");

    cameras->set_center(CameraSlot::Main, Vector2(35.0f, 45.0f));
    cameras->set_zoom(CameraSlot::Main, 2.0f);
    CameraScene::last_instance->request_self_reuse();
    scene_manager.on_update(0.0);

    require(cameras->camera(CameraSlot::Main).center() == Vector2(35.0f, 45.0f),
        "Reuse reload must preserve Main center");
    require(cameras->camera(CameraSlot::Main).zoom() == 2.0f,
        "Reuse reload must preserve Main zoom");

    scene_manager.shutdown();
    require(cameras->camera(CameraSlot::Main).center() == Vector2::zero(),
        "shutdown must reset Main center");
    require(cameras->camera(CameraSlot::Main).zoom() == 1.0f,
        "shutdown must reset Main zoom");
}
}

int main()
{
    test_fixed_slots_are_independent();
    test_requests_are_fifo_and_targeted();
    test_reset_preserves_viewport_and_other_slots();
    test_scene_defaults_and_main_lifecycle();
    reset_cameras();
    std::cout << "camera manager tests passed\n";
    return EXIT_SUCCESS;
}
