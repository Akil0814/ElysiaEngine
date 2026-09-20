# 音频播放

使用 `ELYSIA_AUDIO` 播放已加载音效和音乐。应用负责初始化、逐帧更新和关闭；游戏不要重复调用这些生命周期接口。先在[资源清单](../content/resources.md)中声明 Sound/Music，等待内容加载成功。

## 播放并管理一个音效

以下函数放在游戏代码中，在输入处理或游戏事件中调用。示例 key 需在音频 manifest 中配置；将返回句柄保存为所属技能或场景的成员。

```cpp
#include "engine/audio/audio_service.h"
#include "engine/tools/logger.h"
#include <optional>
#include <chrono>

std::optional<elysia::audio::SoundHandle> play_explosion() {
    using namespace std::chrono_literals;
    auto result = ELYSIA_AUDIO->request_sound("skill.explosion", {
        .group = elysia::audio::SoundGroup::Gameplay,
        .start_delay = 300ms
    });
    if (result.status == elysia::audio::SoundRequestStatus::Rejected) {
        ELYSIA_LOG_WARN("audio", "Explosion sound rejected.");
        return std::nullopt;
    }
    return result.handle;
}

// 技能取消、场景退出或所有者销毁前调用。
void cancel_sound(std::optional<elysia::audio::SoundHandle>& handle) {
    if (!handle) return;
    // false 也可能表示已经自然结束，无需重复停止。
    (void)ELYSIA_AUDIO->stop_sound(*handle);
    handle.reset();
}
```

服务不按场景或对象自动清理播放请求。句柄不延长资源或对象生命；Scheduled 只是预约成功，不保证到期一定播放。
## 音效播放与 SoundHandle

### `play_sound`

```cpp
bool play_sound(const std::string_view& key, int loops = 0);
```

这是兼容性的简化入口：立即请求播放，默认归入 `SoundGroup::Extra`，只返回是否已经实际开始播放。

- `loops = 0`：播放一次。
- `loops > 0`：额外循环次数，实际播放次数为 `loops + 1`。
- `loops = -1`：SDL_mixer 的无限循环语义。

它不返回 handle，因此不适合需要稍后精确停止的持续音效。

### `request_sound`

```cpp
SoundRequestResult request_sound(
    const std::string_view& key,
    const SoundPlayOptions& options = {});
```

这是推荐的音效播放入口。它先验证资源 key，再按 `options` 决定立即播放或创建延迟请求。

```cpp
struct SoundPlayOptions
{
    std::optional<int> loops;                 // nullopt 等同一次播放
    SoundGroup group = SoundGroup::Extra;
    std::chrono::milliseconds start_delay{0};
    std::chrono::milliseconds fade_in{0};
};
```

返回值：

```cpp
enum class SoundRequestStatus { Started, Scheduled, Rejected };

struct SoundRequestResult
{
    SoundRequestStatus status;
    std::optional<SoundHandle> handle;
};
```

- `Started`：channel 已经开始播放，`handle` 指向活跃实例。
- `Scheduled`：请求已进入延迟队列，`handle` 指向待播放实例；到期时仍会检查冷却和并发限制。
- `Rejected`：服务未初始化、资源不存在，或即时请求触发冷却/并发限制；没有 handle。

延迟请求到期时若并发已满、正在冷却或底层播放失败，会被丢弃，不排队、不重试。

### `stop_sound`

```cpp
bool stop_sound(SoundHandle handle, std::chrono::milliseconds fade_out = {});
```

统一停止或取消一个请求：

- handle 仍处于延迟状态：取消待播放请求，返回 `true`。
- handle 已处于播放状态：接受停止请求，返回 `true`。正时长从当前增益渐出，到零后停止 channel 并移除活跃记录；期间 handle 有效且仍占并发名额。
- 渐入中停止会从当前增益渐出；重复渐出请求不重启计时，非正时长立即停止。自然结束会提前清理。
- handle 已自然结束、已被 `ReplaceOldest` 替换、无效，或服务未初始化：返回 `false`。

### `cancel_all_scheduled_sounds`

```cpp
void cancel_all_scheduled_sounds();
```

仅清空尚未开始的延迟请求，不停止已经在播放的音效。与之对应：

```cpp
void stop_all_sounds(std::chrono::milliseconds fade_out = {});
```

仅停止或渐出已经开始的全部音效，不取消延迟请求。所有新增时间参数使用毫秒，默认零；非正时长立即完成。

## 并发组、冷却与溢出策略

每个音效请求属于一个固定 `SoundGroup`：

| 组 | 固定硬上限 | 典型用途 |
| --- | ---: | --- |
| `Ui` | 4 | 焦点、点击、选择、菜单反馈 |
| `Gameplay` | 12 | 角色、技能、受击、脚步等玩法音效 |
| `Ambient` | 4 | 环境与场景持续音 |
| `Extra` | 4 | 暂未分类或通用音效 |

所有组共享 24 个物理 SDL_mixer 音效 channel；音乐不使用这 24 个 channel。四组固定硬上限合计也为 24，因此一个组即使看到其他组空闲，也不能超过自身硬上限。

组规则通过以下类型配置：

```cpp
struct SoundGroupConfig
{
    std::optional<std::size_t> max_simultaneous;
    std::chrono::milliseconds cooldown{0};
    SoundOverflowPolicy overflow_policy = SoundOverflowPolicy::IgnoreNew;
};
```

```cpp
enum class SoundOverflowPolicy
{
    IgnoreNew,
    ReplaceOldest,
};
```

### 规则含义

- `max_simultaneous = std::nullopt`：使用上表固定硬上限。
- 设置具体上限时只能下调，不能超过该组硬上限；`0` 表示该组不接受新音效。
- `cooldown` 按**音效资源 key**计算。同组不同 key 不共享冷却；只有成功开始播放才会刷新该 key 的冷却时间。
- `IgnoreNew`：组已满时拒绝新请求，默认策略，适用于 UI 与环境音。
- `ReplaceOldest`：组已满时停止本组最早开始的活跃声音，再启动新声音；被替换声音的 `SoundHandle` 会失效。它不会停止其他组的声音。
- 若目标组未满但全局 24 channel 已满，仍直接拒绝；当前没有跨组优先级或抢占。

公开配置 API：

```cpp
bool set_sound_group_config(SoundGroup group, const SoundGroupConfig& config);
const SoundGroupConfig& sound_group_config(SoundGroup group) const;
```

当 `max_simultaneous` 超过该组硬上限，或 `cooldown` 为负数时，`set_sound_group_config()` 返回 `false` 且不更新旧配置。

## 音量模型

### 全局与音乐音量

`AudioSettings` 包含：

```cpp
struct AudioSettings
{
    int master_volume = 100;
    int music_volume = 100;
    int sound_volume = 100;
};
```

可在运行时修改：

```cpp
void set_master_volume(int volume);
void set_music_volume(int volume);
void set_sound_volume(int volume);
const AudioSettings& settings() const;
```

所有写入值都会钳制到 `0..100`。

音乐有效音量为：

```text
master_volume / 100 × music_volume / 100 × music_fade_gain × MIX_MAX_VOLUME
```

### 组音量

四个并发组各自拥有不持久化的运行时音量：

```cpp
void set_sound_group_volume(SoundGroup group, int volume);
int sound_group_volume(SoundGroup group) const;
```

- 输入值同样钳制到 `0..100`。
- 新启动的 channel 会立即使用该组有效音量。
- 修改 master、sound 或任一组音量时，该组所有仍活跃的 channel 会立即更新。
- 组音量不进入 `AudioSettings`、用户配置或资源 manifest；下一次初始化 会恢复四组为 `100`。

音效 channel 的有效音量为：

```text
master_volume / 100 × sound_volume / 100 × group_volume / 100 × instance_fade_gain × MIX_MAX_VOLUME
```

渐变增益为独立的 `0..1` 线性系数；计算最终 SDL 音量后统一向下取整。音量设置与渐变相乘，渐变过程中修改设置立即生效，不修改保存的用户音量或组音量。播放前设置初始音量，避免渐入首帧满音量。

`music_volume` 不影响音效；四组 `group_volume` 也不影响音乐。

## 音乐 API

```cpp
bool play_music(const std::string_view& key, int loops = -1,
    std::chrono::milliseconds fade_in = {});
void stop_music(std::chrono::milliseconds fade_out = {});

struct MusicTransitionOptions
{
    int loops = -1;
    std::chrono::milliseconds fade_out{0};
    std::chrono::milliseconds fade_in{0};
};
bool transition_music(const std::string_view& key,
    const MusicTransitionOptions& options = {});
```

- `play_music()` 校验资源后立即替换当前音乐、清除待切换目标，并让新音乐按指定时长渐入；同一 key 也从头播放。
- `transition_music()` 执行“旧音乐渐出 → 启动新音乐 → 新音乐渐入”。无音乐时直接启动新音乐；旧音乐自然结束时，在下一次更新启动目标。
- 仅保存一个待播放目标。渐出期间收到新目标，更新 key、loops 和渐入时长，保留旧音乐渐出进度与结束时间。
- 请求当前仍在播放的同一 key 时，取消待切换目标、保持播放位置与 loops，并按本次渐入时长恢复增益到 1；满音量时继续播放。
- `stop_music()` 清除待切换目标，再从当前增益渐出。重复渐出不重启计时；非正时长立即停止。
- 音乐 key 必须已加载。服务未初始化或资源无效时返回 `false`，不改变当前播放或已接受的切换。默认 `loops = -1` 为持续循环。
- `transition_music()` 返回 `true` 表示请求已接受，不保证未来启动成功。待播放目标持有 key，启动时再次查询资源；资源消失或 SDL 启动失败时记录日志、清空目标并进入空闲，不自动重试。同步启动失败返回 `false`。
- 音乐不参与音效组、冷却、延迟请求、handle 或 24 个音效 channel 的调度。本功能是单路顺序切换，不是两首音乐同时播放的交叉淡化。
- `shutdown()` 立即停止所有播放并清空延迟、切换与渐变状态，不等待渐出；重复初始化先清理上一轮播放状态。


## 生命周期、失败与常见误用

音频更新由 Application 在场景更新之后推进。场景 pause 不会自动暂停全局声音；需要退出某段流程时，显式停止所持句柄或音乐。音量 setter 只改变当前运行状态，保存玩家偏好使用[用户配置](../content/configuration.md)。

`play_music` 返回 false 时保留失败信息并决定是否无音乐继续；`transition_music` 的 true 只代表接受请求，未来启动失败不会自动重试。切换音乐是先淡出后淡入，不是双路交叉淡化。不要靠播放函数的返回值推断整段声音已经播放完成。

改变组规则前检查 `set_sound_group_config` 返回值；非法配置保留旧规则。清理场景专属声音优先停止自己保存的句柄，避免 `stop_all_sounds` 影响其他业务。确需清空全局音效时，分别处理活跃声音和待播放队列。

## 参考

- [音频接口](../../../../engine/audio/audio_service.h)、[播放参数](../../../../engine/audio/sound_playback_types.h)
- [资源加载](../content/resources.md)、[可选架构说明](../../architecture/subsystems/audio.md)、[返回使用指南](../README.md)