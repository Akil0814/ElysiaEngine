# 音频接口与播放契约

本页维护公共参数、结果和生命周期契约。接入方式见[音频指南](../user-guide/systems/audio.md)，内部调度见[架构说明](../architecture/subsystems/audio.md)。定义见 [audio_service.h](../../../engine/audio/audio_service.h)、[sound_playback_types.h](../../../engine/audio/sound_playback_types.h)、[audio_failure.h](../../../engine/audio/audio_failure.h) 和 [scene_sound_types.h](../../../engine/audio/scene_sound_types.h)。

## 公共类型与错误

以下为相关声明摘录，省略命名空间、属性和部分默认成员值，供查询使用；项目直接包含引擎头文件，不重复定义类型。

```cpp
struct AudioFailure
{
    AudioError code;
    elysia::core::FailureDiagnostic diagnostic;
};
template<class T> using AudioResult = std::expected<T, AudioFailure>;

enum class SoundScope { Scene, Global };
enum class SoundPausePolicy { FollowScene, ContinueWhenPaused };
struct SpatialSoundConfig
{
    elysia::core::Vector2 position{};
    SoundDistanceConfig distance;
};
struct SoundPlayOptions
{
    std::optional<int> loops = std::nullopt;
    SoundGroup group = SoundGroup::Extra;
    std::chrono::milliseconds start_delay{0};
    std::chrono::milliseconds fade_in{0};
    double gain = 1.0;
    SoundScope scope = SoundScope::Scene;
    SoundPausePolicy pause_policy = SoundPausePolicy::FollowScene;
    std::optional<SpatialSoundConfig> spatial;
};
enum class SoundRequestStatus { Started, Scheduled };
struct SoundRequestResult
{
    SoundRequestStatus status;
    SoundHandle handle;
};
```

`SoundHandle` 是不持有资源的 `std::uint64_t` 标识。成功结果只有 Started／Scheduled，且句柄非零；失败使用 `std::unexpected<AudioFailure>`。`audio_error_name` 提供错误码名称。

| AudioError | 含义 |
| --- | --- |
| `NotInitialized` | 需要设备服务的操作发生在初始化前或关闭后 |
| `NoActiveScene` | 操作需要当前附着场景，但上下文缺失 |
| `InvalidArgument` | 键、循环次数、增益、位置、距离、枚举或组配置非法 |
| `MissingResource` | 请求的声音或音乐资源尚未加载或已卸载 |
| `InvalidHandle` | 零、外来、已结束或旧生命周期句柄 |
| `CooldownActive` | 资源键仍在请求组的冷却间隔内 |
| `GroupLimitReached` | 组预算用尽且无法按策略替换 |
| `ChannelLimitReached` | 总逻辑声道或实际后端音轨已用尽 |
| `BackendFailure` | 创建、播放、增益、暂停或停止等后端操作失败 |

同步失败由调用方处理，底层不重复记录。诊断保留原始 SDL 错误、来源及资源信息；校验失败或同步后端提交失败不提交新参数。循环源逻辑创建与后续音轨获取的区别见下文。异常内存分配等 C++ 异常不转换成音频错误码。

## 服务接口

```cpp
AudioResult<void> initialize(const AudioSettings& settings);
void shutdown() noexcept;
void update(double delta_seconds);
bool is_initialized() const noexcept;
bool scene_audio_available() const noexcept;

AudioResult<SoundRequestResult> request_sound(
    std::string_view key, const SoundPlayOptions& options = {});
AudioResult<void> stop_sound(SoundHandle handle,
    std::chrono::milliseconds fade_out = {});
AudioResult<void> set_sound_gain(SoundHandle handle, double gain);
AudioResult<void> set_sound_paused(SoundHandle handle, bool paused);
bool sound_active(SoundHandle handle);
void cancel_all_scheduled_sounds();
AudioResult<void> stop_all_sounds(std::chrono::milliseconds fade_out = {});

AudioResult<void> set_sound_group_config(SoundGroup group, const SoundGroupConfig& config);
AudioResult<SoundGroupConfig> sound_group_config(SoundGroup group) const;
AudioResult<void> set_sound_group_volume(SoundGroup group, int volume);
AudioResult<int> sound_group_volume(SoundGroup group) const;
void set_master_volume(int volume);
void set_music_volume(int volume);
void set_sound_volume(int volume);
const AudioSettings& settings() const;

AudioResult<void> play_music(std::string_view key, int loops = -1,
    std::chrono::milliseconds fade_in = {});
AudioResult<void> transition_music(std::string_view key,
    const MusicTransitionOptions& options = {});
AudioResult<void> stop_music(std::chrono::milliseconds fade_out = {});

AudioResult<void> set_listener_position(elysia::core::Vector2 position);
AudioResult<elysia::core::Vector2> listener_position() const;
AudioResult<LoopSoundId> create_loop(const LoopSoundRequest& request);
AudioResult<void> set_loop_position(LoopSoundId id, elysia::core::Vector2 position);
AudioResult<void> set_loop_gain(LoopSoundId id, double gain);
AudioResult<void> set_loop_enabled(LoopSoundId id, bool enabled);
AudioResult<void> destroy_loop(LoopSoundId id);
AudioResult<LoopSoundState> loop_state(LoopSoundId id) const;
AudioResult<void> clear_scene_sounds();
```

### `request_sound`

唯一且无重载的声音请求入口。默认 Scene 归属与 FollowScene 暂停策略；没有附着场景时返回错误。Global 显式选择手动生命周期，场景暂停策略对其无效。Global 与 spatial 同时设置非法。场景只负责持有、附着和更新内部运行时，业务不直接访问运行时。

loops 为空或 0 播放一次，正数为追加次数，-1 无限循环，低于 -1 非法。start_delay／fade_in／fade_out 非正时立即执行。增益接受有限非负值；初始增益在原生播放前设置。

Started 表示已启动逻辑音轨；Scheduled 表示预约已接受，到期才检查冷却、容量和后端。到期失败输出包含资源键、句柄及错误码的结构化诊断并移除预约，不重试或保存完整历史。

### 单实例增益、暂停与停止

`set_sound_gain` 修改预约增益或活动实例基础增益，保留场景距离倍率。`set_sound_paused` 修改独立暂停意图；重复操作幂等。暂停冻结位置、渐变和剩余延迟，仍占声道与组名额。实例暂停与场景暂停叠加。

`stop_sound` 对预约立即取消，对活动声音接受停止或渐出。渐出从当前渐变增益开始，重复正时长停止不延长计时；非正时长立即停止。暂停状态下无论时长均立即停止。渐出声音仍占预算。自然完成、替换、取消、清理后句柄失效；调度器重置或服务重新初始化不会重用旧标识。

`sound_active` 对预约、播放、暂停与渐出返回 true；它不保证可听见。清理失败仍尽力处理其余声音并汇总诊断。

### `cancel_all_scheduled_sounds`

只取消全部预约，无失败返回。`stop_all_sounds` 只处理活动音轨，返回 `AudioResult<void>`，不取消预约。`clear_scene_sounds` 只清理当前场景的活动音轨、预约和逻辑源，并保留监听位置与场景暂停状态。

## 并发组、冷却与溢出策略

| SoundGroup | 硬上限 |
| --- | ---: |
| Ui | 4 |
| Gameplay | 12 |
| Ambient | 4 |
| Extra | 4 |

合计 24 个音效声道，音乐单独计算，空闲组不能借预算给其他组。`SoundGroupConfig` 包含可选 `max_simultaneous`（空值采用硬上限，只能调低）、非负毫秒 `cooldown` 和 `overflow_policy`（IgnoreNew／ReplaceOldest）。组标识和枚举必须有效。

冷却按资源键记录最近成功启动时间，使用本次请求组的冷却时长；同键跨组共享历史，失败不更新历史。冷却在溢出替换之前检查。IgnoreNew 拒绝满组请求；ReplaceOldest 停止本组最早的活动实例后启动新声音，不抢其他组。替换旧实例已停止后，新实例仍可能启动失败。

降低预算不主动停止旧声音；ReplaceOldest 每次最多替换一个，不立即收敛已有超限。显式 0 始终拒绝新声音，包括替换策略。非法配置保留旧值。组查询返回副本；查询不依赖设备初始化。

## 音量模型

AudioSettings 的 master_volume、music_volume、sound_volume 默认均为 100。三项 setter 将输入限制到 0–100，返回 void；内部应用失败记录诊断。`settings()` 返回只读借用引用，保存快照应复制。

组音量默认 100，范围 0–100；设置返回 expected 并验证组标识，后端失败恢复原组参数。组配置、组音量查询均返回带校验的值快照。组配置与音量是运行时设置，不自动持久化。

最终音效增益为 `(master/100) * (sound/100) * (group/100) * instance * fade * distance`；普通声音的 distance 为 1。音乐为 `(master/100) * (music/100) * fade`。更新用户音量不覆盖实例与渐变；有限 double 增益在提交 SDL float 前限制到可表示范围。

## 音乐 API

play_music 校验资源并从头播放，成功后清除待切换目标；同键也重启。transition_music 执行旧音乐渐出、启动目标、目标渐入。MusicTransitionOptions 包含 loops=-1、fade_out=0、fade_in=0。

渐出期间更新目标会替换待播放键、loops 和渐入时长，保留旧渐出截止时间。请求当前仍在播放的同键取消切换，保留位置和 loops，恢复至完整增益。stop_music 清除目标；重复正时长停止不重启渐出。无音乐时切换直接启动，旧音乐自然结束后下一次更新启动目标。

同步失败保留原控制参数与已接受目标；新音轨准备成功后才替换旧音轨。未来启动时重新查找资源；失败记录诊断、清空目标并进入空闲，不自动重试。音乐不参与音效预算、预约或句柄；保持全局单路顺序切换。

## 场景声音源接口

每个 Scene 默认持有音频运行时。SceneRuntimeFeatures.audio 是普通 SceneAudioConfig，loop_retry_interval_seconds 默认 0.25，须有限且至少 0.25；非法场景构造配置抛出 `std::invalid_argument`。服务附着期间可查询监听器，原点为默认位置。监听位置 setter 校验后更新参数，下次场景更新在 on_after_update 之后生效。

SoundDistanceConfig 显式提供正 reference_distance、至少为参考距离的 cutoff_distance、至少为截止距离的 release_distance 和正 exponent，所有值有限；位置也须有限。参考距离内倍率 1，之后为 `(reference_distance / distance)^exponent`，超过截止距离为 0。单位为 EU，位置差以 double 计算。

LoopSoundRequest 包含 key、position、distance、gain=1、group=Ambient、enabled=true 和 pause_policy=FollowScene。create_loop 返回逻辑源句柄，即使暂时缺资源或声道也成功。LoopSoundState 是值快照，包含 status、可选 voice 和可选 last_error。voice 仅用于观察；持久源通过循环源接口控制。

状态为 Playing、Disabled、ScenePaused、Inaudible、OutOfRange、WaitingForVoice、MissingResource。OutOfRange 优先于禁用和场景暂停。截止距离外、释放距离内保留暂停音轨，超过释放距离停止音轨并保留逻辑源；回到可听范围重新播放。竞争声道遵守现有预算、冷却、溢出策略与重试间隔。缺资源不逐帧记录；成功获取音轨或不再需要播放时清除 last_error。

set_loop_position／gain／enabled 校验后提交，同步后端失败恢复原请求参数并返回错误；资源或容量暂时不可用仍以逻辑状态及 last_error 表达。destroy_loop 与 clear_scene_sounds 是清理操作，即使部分停止失败也移除逻辑源并继续其余清理。内部生命周期清理记录汇总失败。

## 生命周期与能力边界

initialize、update、shutdown 由应用调用；初始化失败接入启动失败诊断。关闭与重新初始化停止活动声音、清空预约、循环源与音乐状态，仍存活场景的绑定保持有效，旧源不会恢复。ResourceService 的 has_sound／has_music 是无日志存在性查询，资源卸载停止并脱离关联原生音轨。

场景在用户附着和进入前绑定，在脱离和退出前解绑。退出、复用重置、进入失败、附着回滚与销毁都会清理所属声音，用户回调异常不跳过清理。ContinueWhenPaused 仅豁免暂停。全局声音和音乐继续由调用方负责。

当前仅提供单监听器距离增益；没有声像、遮挡、多普勒、多监听器、跨组抢占、全局满槽等待队列或同时音乐交叉淡化。迁移映射见[指南](../user-guide/systems/audio.md)。
