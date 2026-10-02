#pragma once
#include "engine/scene/scene.h"
#include "engine/audio/audio_service.h"
#include "game/showcase/shared/showcase_frame.h"
#include <array>
namespace example::scene
{
class AudioShowcaseScene final : public elysia::scene::Scene
{
public:
    ~AudioShowcaseScene() override;
protected:
    void on_enter(const elysia::scene::ScenePayload&) override;
    void on_exit() override;
    void on_reset() override;
    void on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>&) override;
private:
    void play(elysia::audio::SoundPlayOptions);
    void stop_sounds();
    void cleanup();
    void report(bool success);
    void back();
    elysia::ui::UiWindow* _window=nullptr;
    example::showcase::ShowcaseFrame _view;
    elysia::scene::SceneRoute _return_route;
    elysia::audio::AudioSettings _saved{};
    std::array<elysia::audio::SoundGroupConfig,elysia::audio::kSoundGroupCount> _groups{};
    std::array<int,elysia::audio::kSoundGroupCount> _volumes{};
    std::vector<elysia::audio::SoundHandle> _handles;
    bool _captured=false,_owns_music=false;
};
}
