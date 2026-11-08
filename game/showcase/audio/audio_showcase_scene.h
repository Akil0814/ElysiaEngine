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
    AudioShowcaseScene();
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
    void report(const elysia::audio::AudioResult<void>& result);
    void report_failure(const elysia::audio::AudioFailure& failure);
    void back();
    elysia::ui::UiWindow* _window=nullptr;
    example::showcase::ShowcaseFrame _view;
    elysia::scene::SceneRoute _return_route;
    elysia::audio::AudioSettings _saved{};
    std::array<elysia::audio::SoundGroupConfig,elysia::audio::kSoundGroupCount> _groups{};
    std::array<int,elysia::audio::kSoundGroupCount> _volumes{};
    std::optional<elysia::audio::LoopSoundId> _spatial_loop;
    bool _spatial_enabled=true;
    bool _captured=false,_owns_music=false;
};
}
