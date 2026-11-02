# 音频播放

本文说明如何查询并播放项目音频，以及如何管理播放句柄、延迟请求和音量。资源是否存在、请求是否被接受和声音是否正在播放是不同状态，下面分别介绍。

使用 `ELYSIA_AUDIO` 播放已加载音效和音乐。应用负责初始化、逐帧更新和关闭；游戏不要重复调用这些生命周期接口。先在[资源清单](../content/resources.md)中声明 Sound/Music，等待内容加载成功。

## 播放并管理一个音效

以下函数放在游戏代码中，在输入处理或游戏事件中调用。示例资源键需在音频资源清单中配置；将返回句柄保存为所属技能或场景的成员。

```cpp
#include "engine/audio/audio_service.h"
#include "engine/tools/logger.h"

#include <optional>
#include <chrono>

std::optional<elysia::audio::SoundHandle> play_explosion()
{
    using namespace std::chrono_literals;
    // 预约由应用的音频调度推进；返回句柄不代表已经发声。
    const elysia::audio::SoundRequestResult result = ELYSIA_AUDIO->request_sound("skill.explosion", {
        .group = elysia::audio::SoundGroup::Gameplay,
        .start_delay = 300ms
    });
    if (result.status == elysia::audio::SoundRequestStatus::Rejected)
    {
        ELYSIA_LOG_WARN("audio", "Explosion sound rejected.");
        return std::nullopt;
    }
    // 调用方保存句柄，在技能取消或退出场景时显式取消预约/停止播放。
    return result.handle;
}

// 技能取消、场景退出或所有者销毁前调用。
void cancel_sound(std::optional<elysia::audio::SoundHandle>& handle)
{
    if (!handle)
        return;
    // false 也可能表示已经自然结束，无需重复停止。
    (void)ELYSIA_AUDIO->stop_sound(*handle);
    handle.reset();
}
```

服务不按场景或对象自动清理播放请求。句柄不延长资源或对象生命；Scheduled 只是预约成功，不保证到期一定播放。

## 播放 UI 提示与配置组规则

下面的局部片段位于已初始化的游戏逻辑中，需包含音频服务和日志头文件。`audio` 是 `elysia::audio::AudioService::instance()` 返回的借用指针；所有毫秒字面量均使用 `std::chrono_literals`。示例键需由项目的音频清单提供。

```cpp
const elysia::audio::SoundRequestResult click = audio->request_sound("ui.button_click", {
    .group = elysia::audio::SoundGroup::Ui
});
if (click.status == elysia::audio::SoundRequestStatus::Rejected)
{
    // 提示音失败不阻止 UI 操作；按项目日志策略记录即可。
    ELYSIA_LOG_WARN("audio", "UI click sound rejected.");
}
```

高频技能音效可选择替换本组最早播放的声音。先设置规则，再发起播放请求；配置失败时保留旧规则。

```cpp
elysia::audio::SoundGroupConfig gameplay_config{};
gameplay_config.max_simultaneous = 8;
gameplay_config.cooldown = 20ms;
gameplay_config.overflow_policy = elysia::audio::SoundOverflowPolicy::ReplaceOldest;
if (!audio->set_sound_group_config(elysia::audio::SoundGroup::Gameplay, gameplay_config))
{
    ELYSIA_LOG_WARN("audio", "Gameplay sound configuration rejected.");
}
```

各组硬上限、按资源键计算的冷却和替换边界见[并发规则](../../reference/audio.md#并发组冷却与溢出策略)。不要用无限重试来绕过播放拒绝。

## 调整环境音与切换音乐

只降低环境音、不影响音乐和技能声音时，使用组音量。以下局部片段沿用上一节的 `audio`：

```cpp
audio->set_sound_group_volume(elysia::audio::SoundGroup::Ambient, 45);
```

组音量只存在于运行时；保存玩家偏好应走[用户配置](../content/configuration.md)。音量范围与最终增益见[音量模型](../../reference/audio.md#音量模型)。

以下片段演示先淡出旧音乐再淡入战斗音乐，并开启循环雨声。它位于宿主的进入逻辑中，`rain_handle` 是宿主保存的 `std::optional<elysia::audio::SoundHandle>` 成员，初始为空；再次进入前必须先清理上次句柄。

```cpp
if (!audio->transition_music("bgm.battle", {
        .fade_out = 800ms,
        .fade_in = 500ms}))
{
    // 校验失败会保留原状态；若同步启动失败，旧音乐可能已经停止。
    ELYSIA_LOG_WARN("audio", "Battle music transition rejected.");
}
const elysia::audio::SoundRequestResult rain = audio->request_sound("ambient.rain", {
    .loops = -1,
    .group = elysia::audio::SoundGroup::Ambient,
    .fade_in = 1000ms
});
rain_handle = rain.handle;
if (rain.status == elysia::audio::SoundRequestStatus::Rejected)
{
    ELYSIA_LOG_WARN("audio", "Rain sound rejected.");
}
```

离开这段流程时执行下面的片段。渐出请求提交后不必继续持有句柄；这也避免下次进入误用旧标识。

```cpp
if (rain_handle)
{
    (void)audio->stop_sound(*rain_handle, 800ms);
    rain_handle.reset();
}
```

音乐切换返回成功只代表请求被接受，未来启动仍可能失败；完整规则见[音乐接口](../../reference/audio.md#音乐-api)。若业务需要两首音乐同时交叉淡化，当前单路音乐接口不提供这项能力。

## 清理与排查失败

音频调度由应用在场景更新后推进，延迟、冷却和渐变受全局时间倍率影响；场景暂停不会自动暂停全局声音。慢动作也不会自动改变已经启动的音轨的音高或播放速度。

先检查服务初始化和资源键，再区分请求被拒绝、预约已接受和实际开始播放。预约到期仍可能因冷却、并发或底层播放失败而被丢弃。不要把播放返回值当作声音已播放完成的通知。

场景退出、技能取消或宿主销毁前，停止自己保存的句柄。需要清空全局音效时，要分别处理活跃声音和待播放队列；两者的接口区别见[停止与取消](../../reference/audio.md#cancel_all_scheduled_sounds)。

## 控制一个声音的增益与暂停

`SoundPlayOptions::gain` 是独立于玩家音量的非负实例增益，默认 1。保存返回句柄后，可用 `set_sound_gain` 动态修改，用 `set_sound_paused(handle, true/false)` 暂停或恢复。`sound_active` 包含预约、暂停和渐出状态；它不能作为“此刻可听见”的判断。

暂停会冻结该声音的播放位置与渐变；预约尚未开始时冻结剩余等待时间。停止一个暂停的声音会立即释放，无需先恢复。全局服务仍由应用更新，游戏不要手动再推进一次。

## 让场景管理声音

需要随场景暂停和退出清理时，通过 `SceneRuntimeFeatures::audio` 启用运行时，再从派生 Scene 调用 `audio_runtime()`。普通声音使用 `request_sound`；指定世界位置的声音使用 `request_sound_at`；需要距离休眠与重新启动的循环源使用 `create_loop`。前两者返回普通播放结果，循环源返回 `LoopSoundId`。

下面的完整类要求清单已经加载 `ambient.wind` 音效。参数中的位置和距离使用 EU，监听器由游戏更新，不自动跟随相机。示例参数由项目选择，并非引擎默认距离。

```cpp
#include "engine/scene/scene.h"

class WindScene final : public elysia::scene::Scene
{
public:
    WindScene() : Scene(elysia::scene::SceneRuntimeFeatures{
        .audio = elysia::scene::SceneAudioConfig{}}) {}

    void move_listener(elysia::core::Vector2 position)
    {
        (void)audio_runtime().set_listener_position(position);
    }

protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        (void)audio_runtime().set_listener_position({0, 0});
        wind = audio_runtime().create_loop({
            .key = "ambient.wind",
            .position = {100, 0},
            .distance = {50, 400, 480, 1.4},
            .gain = 0.5
        });
    }
    void on_exit() override { wind.reset(); }
    void on_reset() override { wind.reset(); }

private:
    std::optional<elysia::scene::LoopSoundId> wind;
};
```

运行时在 `on_after_update` 后刷新监听距离。一次性声音持续调整增益，结束后不重新播放。循环声超过可听截止距离 400 时暂停并保留位置，超过释放距离 480 时释放声道；返回可听范围后从头播放。`set_loop_enabled` 与场景暂停分别记录，恢复场景不会启用已禁用源。

默认声音跟随场景暂停；UI 提示可向 `request_sound` 传入 `SceneSoundPausePolicy::ContinueWhenPaused`。运行时只清理通过它创建的声音，不停止其他场景、全局声音或音乐。退出、重置、进入失败和销毁均自动清理；复用场景时在 `on_enter` 重新创建声音源。

`loop_state` 是最近一次更新或声音源 setter 后的快照，不触发播放。`WaitingForVoice` 表示尚未取得声道，运行时最多每 250 ms 重试一次；`MissingResource` 表示资源缺失，同一缺失阶段只记录一次诊断。总声道仍为 24，Ambient 组默认最多 4 个实际音轨，逻辑源不额外预留声道。

## 从 FlyMe2TheMoon 接入

项目的声音初始增益、`set_sound_gain`、`set_sound_paused` 和 `sound_active` 可直接对应公开服务接口。将 `World::set_listener` 改为设置场景监听器；将 `World::at` 映射到 `request_sound_at`，将按 owner 维护的循环声映射到游戏保存的 `LoopSoundId`，更新位置、增益和启用状态即可。

距离模型由项目转换成 `SoundDistanceConfig`，角色语音、水晶连吃规则、资源键映射和原作参数仍留在游戏。旧项目需要先移除同一声音的重复手动暂停与清理路径；全局音乐仍由项目决定何时停止。本次未修改 FlyMe2TheMoon，也未扩大引擎声道预算。

## 继续阅读

- [音频接口与播放契约](../../reference/audio.md)：完整参数、结果和能力边界。
- [音频架构](../../architecture/subsystems/audio.md)：调度时序、轨道后端与渐变流程。
- [资源加载](../content/resources.md)、[返回使用指南](../README.md)。
