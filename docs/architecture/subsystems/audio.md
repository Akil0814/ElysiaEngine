# AudioService 模块说明

`elysia::audio::AudioService` 是 Elysia Engine 的运行时音频播放服务。它负责把已经加载好的音频资源播放为音效或音乐，并管理音效的并发、冷却、延迟、停止和运行时音量、渐入渐出与音乐顺序切换。

本文面向两类读者：

- gameplay/UI 调用者：需要知道如何请求播放、配置组规则、停止指定声音与调整音量。
- 模块维护者：需要理解 `AudioService`、`SoundPlaybackScheduler`、`ResourceManager` 与 SDL_mixer 的职责边界。

## 1. 模块边界

音频流程分为资源持有、运行时调度与底层播放三层：

```text
音频 manifest / 内容加载
        │
        ▼
ResourceManager / AudioManager
  按 key 持有 MIX_Audio（音效与音乐分别索引）
        │
        ▼
AudioService
  校验资源、调度请求、音量控制、调用 SDL_mixer
        │
        ▼
SDL_mixer
  Mix_PlayChannel / Mix_PlayMusic / Mix_HaltChannel
```

- `ResourceManager` 负责通过资源 key 查找已加载的 `MIX_Audio`（音效预解码、音乐按需解码），不负责播放策略。
- `AudioService` 是 gameplay 与 UI 的公开播放入口；它不依赖 `Scene`，也不持有场景对象。
- `SoundPlaybackScheduler` 是 `AudioService` 的内部调度器，维护音效的待播放/活跃状态、`SoundHandle`、并发组、冷却和溢出策略。
- SDL_mixer 负责实际 channel 和音乐播放。SDL 资源查找、停止 channel、设置 channel 音量仍由 `AudioService` 处理。

## 2. 生命周期与更新顺序

应用初始化 SDL_mixer 音频设备后调用：

```cpp
audio->initialize(runtime_settings.audio);
```

`init()` 会：

1. 将 master/music/sound 音量钳制到 `0..100`。
2. 显式分配 24 个 SDL_mixer 音效 channel。
3. 清空调度器运行时状态。
4. 将四个组的运行时音量重置为 `100`。
5. 应用音乐和当前活跃音效的有效音量。

主循环每帧应调用：

```cpp
audio->update(delta_seconds);
```

该调用推进延迟请求及音乐、音效的线性渐变；到期请求会在此时尝试正式播放。负数或非有限 delta 按零处理。新启动的声音从实际开始播放时计时，不消耗启动前的帧时间。`Application` 当前在场景更新和场景切换处理完成后调用它，因此音频服务不需要知道场景生命周期。

关闭时调用：

```cpp
audio->shutdown();
```

它会停止音乐、停止已开始的全部音效并清空待播放与活跃调度状态。

## 3. 音效播放与 SoundHandle

### `play_sound`

```cpp
bool play_sound(std::string_view key, int loops = 0);
```

这是兼容性的简化入口：立即请求播放，默认归入 `SoundGroup::Extra`，只返回是否已经实际开始播放。

- `loops = 0`：播放一次。
- `loops > 0`：额外循环次数，实际播放次数为 `loops + 1`。
- `loops = -1`：SDL_mixer 的无限循环语义。

它不返回 handle，因此不适合需要稍后精确停止的持续音效。

### `request_sound`

```cpp
SoundRequestResult request_sound(
    std::string_view key,
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

## 4. 并发组、冷却与溢出策略

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

## 5. 音量模型

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
- 组音量不进入 `AudioSettings`、用户配置或资源 manifest；下一次 `init()` 会恢复四组为 `100`。

音效 channel 的有效音量为：

```text
master_volume / 100 × sound_volume / 100 × group_volume / 100 × instance_fade_gain × MIX_MAX_VOLUME
```

渐变增益为独立的 `0..1` 线性系数；计算最终 SDL 音量后统一向下取整。音量设置与渐变相乘，渐变过程中修改设置立即生效，不修改保存的用户音量或组音量。播放前设置初始音量，避免渐入首帧满音量。

`music_volume` 不影响音效；四组 `group_volume` 也不影响音乐。

## 6. 音乐 API

```cpp
bool play_music(std::string_view key, int loops = -1,
    std::chrono::milliseconds fade_in = {});
void stop_music(std::chrono::milliseconds fade_out = {});

struct MusicTransitionOptions
{
    int loops = -1;
    std::chrono::milliseconds fade_out{0};
    std::chrono::milliseconds fade_in{0};
};
bool transition_music(std::string_view key,
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

## 7. 常用调用示例

以下示例假定：

```cpp
using namespace std::chrono_literals;
auto* audio = elysia::audio::AudioService::instance();
```

### UI 即时音效

```cpp
audio->request_sound("ui.button_click", {
    .group = elysia::audio::SoundGroup::Ui
});
```

### 可取消的 Gameplay 延迟音效

```cpp
const auto result = audio->request_sound("skill.explosion", {
    .group = elysia::audio::SoundGroup::Gameplay,
    .start_delay = 300ms
});

// 技能被打断或所属对象销毁时：
if (result.handle)
    audio->stop_sound(*result.handle);
```

### 高频 Gameplay 音效采用最新优先

```cpp
elysia::audio::SoundGroupConfig gameplay_config{};
gameplay_config.max_simultaneous = 8;
gameplay_config.cooldown = 20ms;
gameplay_config.overflow_policy = elysia::audio::SoundOverflowPolicy::ReplaceOldest;

if (!audio->set_sound_group_config(
        elysia::audio::SoundGroup::Gameplay,
        gameplay_config))
{
    // 上限非法时保留旧配置；按项目日志策略处理。
}
```

### 单独降低环境音

```cpp
audio->set_sound_group_volume(elysia::audio::SoundGroup::Ambient, 45);
```

### 音乐平滑切换与环境音渐变

```cpp
audio->transition_music("bgm.battle", {
    .fade_out = 800ms,
    .fade_in = 500ms
});
const auto rain = audio->request_sound("ambient.rain", {
    .loops = -1,
    .group = elysia::audio::SoundGroup::Ambient,
    .fade_in = 1000ms
});
if (rain.handle)
    audio->stop_sound(*rain.handle, 800ms);
```

## 8. 内部调度器职责

`SoundPlaybackScheduler` 不作为 gameplay/UI 的直接入口。它完成以下工作：

- 生成并维护 `SoundHandle`；同一 handle 会从 Pending 延续到 Playing。
- 维护待播放项的到期时间与活跃 channel 记录。
- 每次请求和更新前清理 SDL 已结束的 channel，避免并发计数残留。
- 判定冷却、组上限、全局 channel 上限与溢出策略。
- 在 `ReplaceOldest` 时选择同组最早活跃项并通过回调停止其 channel。
- 保存每个活跃实例的渐变与停止状态，按组枚举 channel 及增益，供 `AudioService` 更新实际音量。

`AudioService` 向调度器传入“启动声音、检查 channel 是否仍播放、停止 channel”的回调。启动回调接收初始增益，更新回调应用增益。内部 `MusicPlaybackController` 同样通过播放、停止、播放状态和音量回调管理音乐，不缓存资源指针。共享 `AudioFade` 提供线性插值。这样调度器不依赖 `Scene`、`ResourceManager` 或 SDL_mixer 的资源查找；SDL 细节仍集中在服务层。

## 9. 当前能力边界

当前模块**不提供**：

- 满槽后等待、自动重试或顺序播放队列。
- 跨组优先级与跨组 channel 抢占。
- 自定义动态音效组。
- 单实例暂停/恢复、声像、音高或随机变体。
- 多路音乐交叉淡化、音乐排队、组音量渐变，以及内置启动画面播放器的渐变扩展。
- 组音量的用户设置持久化。
- 自动按 Scene、实体或技能批量取消延迟请求。

这些能力应在出现明确玩法或产品需求时单独设计，避免把资源加载、场景生命周期和运行时播放调度重新耦合。
