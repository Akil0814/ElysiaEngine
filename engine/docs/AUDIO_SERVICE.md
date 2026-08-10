# AudioService 模块说明

`elysia::audio::AudioService` 是 Moonline 的运行时音频播放服务。它负责把已经加载好的音频资源播放为音效或音乐，并管理音效的并发、冷却、延迟、停止和运行时音量。

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
  按 key 持有 Mix_Chunk 与 Mix_Music
        │
        ▼
AudioService
  校验资源、调度请求、音量控制、调用 SDL_mixer
        │
        ▼
SDL_mixer
  Mix_PlayChannel / Mix_PlayMusic / Mix_HaltChannel
```

- `ResourceManager` 负责通过资源 key 查找已加载的 `Mix_Chunk`（音效）和 `Mix_Music`（音乐），不负责播放策略。
- `AudioService` 是 gameplay 与 UI 的公开播放入口；它不依赖 `Scene`，也不持有场景对象。
- `SoundPlaybackScheduler` 是 `AudioService` 的内部调度器，维护音效的待播放/活跃状态、`SoundHandle`、并发组、冷却和溢出策略。
- SDL_mixer 负责实际 channel 和音乐播放。SDL 资源查找、停止 channel、设置 channel 音量仍由 `AudioService` 处理。

## 2. 生命周期与更新顺序

应用初始化 SDL_mixer 音频设备后调用：

```cpp
audio->init(runtime_settings.audio);
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

该调用推进延迟请求的时间；到期请求会在此时尝试正式播放。Moonline 当前在场景更新和场景切换处理完成后调用它，因此音频服务不需要知道场景生命周期。

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
bool stop_sound(SoundHandle handle);
```

统一停止或取消一个请求：

- handle 仍处于延迟状态：取消待播放请求，返回 `true`。
- handle 已处于播放状态：停止其 SDL channel 并移除活跃记录，返回 `true`。
- handle 已自然结束、已被 `ReplaceOldest` 替换、无效，或服务未初始化：返回 `false`。

### `cancel_all_scheduled_sounds`

```cpp
void cancel_all_scheduled_sounds();
```

仅清空尚未开始的延迟请求，不停止已经在播放的音效。与之对应：

```cpp
void stop_all_sounds();
```

仅停止已经开始的全部音效，不取消延迟请求。

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
master_volume × music_volume / 100
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
master_volume × sound_volume × group_volume / 10000
```

`music_volume` 不影响音效；四组 `group_volume` 也不影响音乐。

## 6. 音乐 API

```cpp
bool play_music(std::string_view key, int loops = -1);
void stop_music();
```

- `play_music()` 在开始新音乐前停止当前音乐。
- 默认 `loops = -1`，即持续循环。
- 音乐资源必须已由 `ResourceManager` 按 key 加载；服务未初始化、资源不存在或 SDL_mixer 返回错误时，函数返回 `false`。
- 音乐是单独播放路径，不参与 `SoundGroup`、冷却、延迟请求、`SoundHandle` 或 24 个音效 channel 的调度。

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

## 8. 内部调度器职责

`SoundPlaybackScheduler` 不作为 gameplay/UI 的直接入口。它完成以下工作：

- 生成并维护 `SoundHandle`；同一 handle 会从 Pending 延续到 Playing。
- 维护待播放项的到期时间与活跃 channel 记录。
- 每次请求和更新前清理 SDL 已结束的 channel，避免并发计数残留。
- 判定冷却、组上限、全局 channel 上限与溢出策略。
- 在 `ReplaceOldest` 时选择同组最早活跃项并通过回调停止其 channel。
- 按组枚举活跃 channel，供 `AudioService` 更新实际 channel 音量。

`AudioService` 向调度器传入“启动声音、检查 channel 是否仍播放、停止 channel”的回调。这样调度器不依赖 `Scene`、`ResourceManager` 或 SDL_mixer 的资源查找；SDL 细节仍集中在服务层。

## 9. 当前能力边界

当前模块**不提供**：

- 满槽后等待、自动重试或顺序播放队列。
- 跨组优先级与跨组 channel 抢占。
- 自定义动态音效组。
- 单实例暂停/恢复、淡入淡出、声像、音高或随机变体。
- 组音量的用户设置持久化。
- 自动按 Scene、实体或技能批量取消延迟请求。

这些能力应在出现明确玩法或产品需求时单独设计，避免把资源加载、场景生命周期和运行时播放调度重新耦合。
