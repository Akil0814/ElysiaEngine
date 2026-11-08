# 音频播放

使用 `ELYSIA_AUDIO` 配置音量、请求音效和控制声音源。普通请求默认归当前附着场景所有：离开、重置或销毁场景时，引擎清理这些声音和预约。空间声音也走同一个请求入口，只需提供完整的位置和距离配置。

应用负责初始化、逐帧更新和关闭音频服务。游戏先在[资源清单](../content/resources.md)声明音效和音乐，等待内容加载成功，再在场景进入、更新或输入回调中请求播放。

## 从场景发出请求

每个 `Scene` 默认持有音频运行时，无需启用开关。`scene_audio_available()` 表示当前是否附着了场景上下文；`is_initialized()` 表示设备服务是否就绪。正常 SceneManager 流程在用户附着回调和 `on_enter` 前绑定，在脱离和 `on_exit` 回调前解绑。无上下文的默认请求返回 `NoActiveScene`，不会自动改变归属。

下面的完整场景要求清单已经加载 `ambient.wind`、`skill.explosion` 和 `ui.prompt`。示例距离由项目选择，单位是 EU。监听器由游戏显式更新，不自动跟随相机。

```cpp
#include "engine/scene/scene.h"
#include "engine/audio/audio_service.h"
#include "engine/tools/logger.h"
#include <optional>
#include <chrono>

class WindScene final : public elysia::scene::Scene
{
public:
    void move_listener(elysia::core::Vector2 position)
    {
        check(ELYSIA_AUDIO->set_listener_position(position));
    }

    void explosion(elysia::core::Vector2 position)
    {
        auto result = ELYSIA_AUDIO->request_sound("skill.explosion", {
            .group = elysia::audio::SoundGroup::Gameplay,
            .gain = 0.8,
            .spatial = elysia::audio::SpatialSoundConfig{
                position, {50, 400, 480, 1.4}}
        });
        if (!result) report(result.error());
    }

    void ui_prompt()
    {
        auto result = ELYSIA_AUDIO->request_sound("ui.prompt", {
            .group = elysia::audio::SoundGroup::Ui,
            .pause_policy = elysia::audio::SoundPausePolicy::ContinueWhenPaused
        });
        if (!result) report(result.error());
    }

protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        auto result = ELYSIA_AUDIO->create_loop({
            .key = "ambient.wind",
            .position = {100, 0},
            .distance = {50, 400, 480, 1.4},
            .gain = 0.5
        });
        if (result) wind = *result;
        else report(result.error());
    }
    void on_exit() override { wind.reset(); }
    void on_reset() override { wind.reset(); }

private:
    static void report(const elysia::audio::AudioFailure& failure)
    {
        ELYSIA_LOG_WARN("game.audio",
            elysia::audio::audio_error_name(failure.code) << ": "
            << failure.diagnostic.message);
    }
    static void check(const elysia::audio::AudioResult<void>& result)
    {
        if (!result) report(result.error());
    }
    std::optional<elysia::audio::LoopSoundId> wind;
};
```

`request_sound` 成功时提供 `Started` 或 `Scheduled` 和非零句柄。失败时读取 `result.error().code` 和 `diagnostic`。这次请求接受成功不等于声音已经播放完毕；预约只表示已入队，到期仍可能失败。

`SoundPlayOptions` 不提供 `spatial` 时播放普通声音。提供时必须完整设置距离：参考距离内增益为 1，之后按 `(reference_distance / distance)^exponent` 衰减，超过截止距离为 0。位置和距离必须有限，参考距离大于 0，截止距离不小于参考距离，释放距离不小于截止距离，指数大于 0。

一次性位置声音随监听器移动更新增益，自然结束后移除，距离变化不会触发重播。持久空间循环声使用 `create_loop`：超过截止距离暂停音轨并保留播放位置，超过释放距离释放声道；重新进入可听范围后从头开始。

## 控制声音与暂停

保存 `result->handle` 后，通过 `set_sound_gain`、`set_sound_paused` 和 `stop_sound` 控制声音。实例增益接受有限非负值；它与主、音效、组音量及渐变相乘。全局音量变化不会覆盖实例增益，服务也会保留所属声音的基础增益和独立暂停意图。

场景声音默认跟随场景暂停。示例 UI 提示使用 `SoundPausePolicy::ContinueWhenPaused`，仍归场景清理，但暂停时继续播放。单实例暂停与场景暂停叠加；恢复场景不会恢复手动暂停的声音，也不会启用已禁用循环源。全局声音忽略场景暂停策略。

暂停冻结播放位置、渐变和预约剩余等待时间；暂停音轨仍占声道与组预算。停止暂停声音时立即释放，即使指定正数渐出时间。`sound_active` 包含预约、暂停和渐出，并不表示此刻可听见。自然结束、替换或清理后旧句柄失效，服务重启也不会复用旧句柄。

循环源使用独立的 `LoopSoundId`，可通过 `set_loop_position`、`set_loop_gain`、`set_loop_enabled` 修改，通过 `loop_state` 查询快照，通过 `destroy_loop` 销毁。创建成功仅代表逻辑源成立；`WaitingForVoice`、`MissingResource` 和 `last_error` 说明未取得音轨的原因。成功取得音轨或不再需要播放时清除最近错误，同一失败阶段不会逐帧输出日志。暂时无声道时默认最多每 250 ms 重试一次；可在 `SceneRuntimeFeatures::audio` 的 `SceneAudioConfig` 中调大间隔。

## 全局声音、组规则与音乐

需要跨场景保留或无场景播放的声音显式设置 `scope = SoundScope::Global`，并由调用方停止。内置按钮、复选框、单选框和滑块提示音采用这种归属。`Global + spatial` 返回 `InvalidArgument`，因为监听器属于场景。普通 UI 提示也可保留默认场景归属，按需要选暂停策略。

组配置和组音量都通过服务设置并检查 `AudioResult<void>`；查询返回值快照。总声道仍是 24，组硬上限为 Ui 4、Gameplay 12、Ambient 4、Extra 4。组冷却与溢出策略见[接口参考](../../reference/audio.md)。不要通过逐帧重试普通请求绕过容量拒绝。

主、音乐和音效音量设置保留 0–100 限制范围的无失败契约；组音量设置还会校验组标识并提交到后端。组配置只存在于运行时，玩家偏好通过[用户配置](../content/configuration.md)保存。

`play_music`、`transition_music` 和 `stop_music` 返回 `AudioResult<void>`。音乐保留全局单路语义：切换先淡出再淡入，不同时交叉淡化。同步验证或启动失败保留原控制状态；已接受切换未来启动失败时记录诊断并结束该切换。

## 失败与清理

同步错误由调用方处理或明确忽略，底层不重复记录。`AudioError` 区分初始化、场景上下文、参数、资源、句柄、冷却、组容量、总声道和后端错误；完整结果类型和逐接口契约见[音频参考](../../reference/audio.md)。预约到期失败记录资源键、句柄和错误码后移除，不无限重试，也不保存完整播放历史。

场景更新末尾、`on_after_update` 之后刷新距离与声音状态。应用在场景更新后推进全局调度，时间倍率影响延迟、冷却和渐变；不会改变原生播放速度或音高。游戏不要重复调用服务的 `update`。

场景退出、复用重置、进入失败、运行时脱离与销毁会清理所属活动声音、预约和逻辑源，用户生命周期回调抛异常也执行清理。`clear_scene_sounds()` 可主动清理当前场景，并保留监听位置和暂停状态。全局声音与音乐需要手动管理：`stop_all_sounds` 处理活动音轨，`cancel_all_scheduled_sounds` 单独取消全部预约。清理尝试处理所有声音，汇总失败诊断。

## 更新旧接口与 FlyMe2TheMoon 接入

这是一次破坏性更新，没有弃用包装。尤其要检查旧 `request_sound` 调用：以前手动管理的全局声音现在默认归当前场景。

| 原用法 | 新用法 |
| --- | --- |
| `play_sound(key, loops)` | `request_sound(key, {.loops = loops})`，检查 expected |
| 旧 `request_sound` 的全局行为 | 显式配置 `.scope = SoundScope::Global` |
| `request_scene_sound` | 默认 `request_sound` |
| `request_sound_at` / `World::at` | `request_sound` 的 `.spatial = SpatialSoundConfig{position, distance}` |
| `SceneSoundPausePolicy` | `SoundPausePolicy`，放进 options 或循环源请求 |
| `Rejected` / 可选结果句柄 | `!result` / `result.error()`；成功使用 `result->handle` |
| `bool` 控制返回值 | `AudioResult<void>`，具体原因在 `error()` |
| `has_audio` / `try_audio_runtime` / `audio_runtime` | 移除；服务检查 `scene_audio_available()` |
| 场景音频可选启用 | 默认提供；仅保留 `SceneAudioConfig` 参数配置 |

下面是 FlyMe2TheMoon 的迁移适配示例。返回普通声音句柄即可用于现有取消流程；可选全局 UI 反馈明确忽略结果。

```cpp
#include "engine/audio/audio_service.h"

elysia::audio::AudioResult<elysia::audio::SoundHandle> play_world_at(
    std::string_view key, elysia::core::Vector2 position,
    const elysia::audio::SoundDistanceConfig& distance)
{
    auto result = ELYSIA_AUDIO->request_sound(key, {
        .group = elysia::audio::SoundGroup::Gameplay,
        .spatial = elysia::audio::SpatialSoundConfig{position, distance}
    });
    if (!result) return std::unexpected(result.error());
    return result->handle;
}

void optional_global_click()
{
    (void)ELYSIA_AUDIO->request_sound("ui.click", {
        .group = elysia::audio::SoundGroup::Ui,
        .scope = elysia::audio::SoundScope::Global
    });
}
```

将 `World::set_listener` 对应到 `set_listener_position`；owner 循环声改为保存 `LoopSoundId` 并更新位置、增益、启用状态。移除同一场景声音的重复手动暂停与清理逻辑。距离模型由项目转换成 `SoundDistanceConfig`；角色语音、水晶连吃、资源键和具体参数继续属于游戏。本轮未修改 FlyMe2TheMoon。

## 继续阅读

- [音频接口与播放契约](../../reference/audio.md)
- [音频架构](../../architecture/subsystems/audio.md)
- [资源加载](../content/resources.md)、[返回使用指南](../README.md)
