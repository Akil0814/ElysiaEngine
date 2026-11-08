# 音频架构与调度流程

本页面向维护音频实现的开发者，解释资源持有、请求调度、轨道播放和渐变的协作。游戏接入见[音频指南](../../user-guide/systems/audio.md)，完整公开契约见[音频参考](../../reference/audio.md)。

## 模块边界

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
  MIX_Track / MIX_PlayTrack / MIX_StopTrack
```

- `ResourceManager` 负责通过资源键查找已加载的 `MIX_Audio`（音效预解码、音乐按需解码），不负责播放策略。
- `AudioService` 是游戏玩法与 UI 的统一入口，持有全局调度器，并借用当前附着的 SceneAudioRuntime 转发场景请求；不拥有 Scene 对象。
- `SoundPlaybackScheduler` 是 `AudioService` 的内部调度器，维护音效的待播放/活跃状态、`SoundHandle`、并发组、冷却和溢出策略。
- SDL3_mixer 使用轨道完成实际播放。调度器中的逻辑通道是引擎逻辑通道索引，资源查找与轨道操作由服务及其后端适配完成，不是 SDL2_mixer 的通道 API。

## 生命周期与一帧更新

应用先调用 `MIX_Init` 初始化 SDL_mixer，再检查 `AudioService::initialize` 的 `AudioResult<void>`，失败诊断进入现有启动失败路径，终止依赖音频的后续初始化。服务通过共享 `MixerBackend` 创建或复用音频设备、音效轨道与独立音乐轨道，重置播放调度和组音量，再应用初始音量；重复初始化先清理上一轮状态。

应用每帧在场景更新和场景切换处理完成后，以缩放后的 delta 调用 `AudioService::update`。服务先推进 `SoundPlaybackScheduler`，再推进 `MusicPlaybackController`；负数或非有限 delta 按零处理。更新过程中到期的预约和切换目标在旧实例的渐变处理后启动，其渐入不再消耗本次更新的 delta。底层音频设备独立播放已启动的轨道，因此调度时钟变化不会自动改变音轨播放速度。

关闭时，服务停止所有轨道，清空预约、活跃实例、音乐切换目标及渐变状态；关闭不等待渐出。应用随后清理资源，并调用共享后端的 `shutdown` 销毁设备和轨道。全局请求不自动继承场景、实体或技能寿命；场景请求经内部运行时清理。

## 音效调度与音乐状态机

`SoundPlaybackScheduler` 不作为游戏玩法与 UI 的直接入口。它完成以下工作：

- 生成并维护 `SoundHandle`；同一句柄会从 Pending 延续到 Playing。
- 维护待播放项的到期时间与活跃逻辑通道记录。
- 尝试启动、停止或更新音效时清理 SDL 已结束的逻辑通道，避免并发计数残留。
- 判定冷却、组上限、全局逻辑通道上限与溢出策略。
- 在 `ReplaceOldest` 时选择同组最早活跃项并通过回调停止其逻辑通道。
- 保存每个活跃实例的渐变与停止状态，按组枚举逻辑通道及增益，供 `AudioService` 更新实际音量。

`AudioService` 向调度器传入“启动声音、检查逻辑通道是否仍播放、停止逻辑通道”的回调。启动回调接收初始增益，更新回调应用增益。内部 `MusicPlaybackController` 同样通过播放、停止、播放状态和音量回调管理音乐，不缓存资源指针。共享 `AudioFade` 提供线性插值。这样调度器不依赖 `Scene`、`ResourceManager` 或 SDL_mixer 的资源查找；SDL 细节仍集中在服务层。

## 从增益计算到轨道播放

调度器管理实例的渐变系数，`AudioService` 将它与当前用户音量、音效组音量相乘。音乐和音效最终通过 `MIX_SetTrackGain` 写入浮点轨道增益；计算关系只在[音量参考](../../reference/audio.md#音量模型)维护。

启动轨道前先设置初始增益，避免渐入首帧满音量。音乐替换先在临时音轨上完成初始增益与启动，成功后才提交新音轨指针并销毁旧音轨；同步准备失败保留原播放状态。设置变化时重新应用活跃轨道增益，不改写调度器中的渐变进度。音乐切换控制器只保存待播放资源键，在实际启动时再次查找资源；资源已消失或底层启动失败时记录日志并清空目标，不进入自动重试循环。

## 场景声音所有权

Scene 持有默认的 `SceneAudioRuntime`，后者保存资源键、普通声音句柄、循环逻辑源和监听位置。游戏入口统一为 AudioService：附着时借用运行时指针，脱离时先解绑再执行用户回调；一次只能附着一个运行时。运行时不持有资源或游戏对象指针，资源仍归 ResourceManager 所有。Scene 在游戏 `on_after_update` 后推进距离更新，再由应用推进全局音频调度。

场景暂停同步到所属声音，分别冻结预约和活动渐变。场景暂停、单实例暂停与循环源禁用分别记录，恢复场景只解除场景原因。唯一的 request_sound 默认归场景并跟随暂停；SoundPlayOptions 可显式选择 Global 或提供 spatial 配置。Global 忽略场景暂停，且不能带 spatial，因为监听器归属场景。实例 setter 自动识别归属；运行时通过私有播放、增益和暂停路径避免递归与刷新覆盖。清理位于非虚生命周期入口，覆盖重置和失败，不依赖用户调用基类回调。

逻辑循环源与实际音轨分离：不可听带内保留暂停音轨，超过释放距离停止并归还声道，返回可听范围再取得新音轨。创建成功表示逻辑源成立，后续获取失败通过 status 和 last_error 查询。重试默认最多每 250 ms 一次，继续遵守调度器预算、冷却和溢出策略；取得音轨或不再需要播放时清除最近错误。缺失资源的同一阶段不重复输出诊断。

SoundHandle 和 LoopSoundId 使用各自进程级单调标识，重置不会复用；不同场景及服务重启后的旧句柄不会命中新声音。调度器后端将暂停轨道视为仍占用的轨道，并在启动前应用实例增益。新增回归见 `sound_instance_control_tests`、`scene_audio_runtime_tests` 和 `audio_service_scene_tests`；自动化使用 dummy 设备，不构成实际试听。

## 错误传递与状态提交

AudioFailure 将具体 AudioError 与通用 FailureDiagnostic 组合。调度器的启动、停止、增益、暂停以及音乐回调传递 expected 错误，避免压缩成 bool。SDL 错误在清理覆盖前捕获；has_sound 和 has_music 提供无日志存在性查询。同步错误交给调用方，校验后的参数与渐变进度在后端成功提交后更新。清理尽力处理全部所属声音并汇总诊断。

Scheduled 只确认预约已接受。到期失败补充资源键与句柄，按错误码名称记录一次并移除预约，不保存完整播放历史。源获取音轨走服务私有播放路径，跳过公开路由但仍遵守预算。所属声音 setter 走私有增益与暂停路径，维护归属参数并避免递归。

SceneAudioConfig 始终存在，仅配置重试间隔。服务借用唯一的已附着运行时，业务不直接访问运行时。scene_audio_available 反映附着状态，与设备就绪独立。内置 UI 反馈显式选择 Global，从而在场景附着前也能播放。

## 源码与测试入口

- [audio_service.cpp](../../../../engine/audio/audio_service.cpp)：资源校验、轨道操作、调度回调和浮点增益应用。
- [mixer_backend.h](../../../../engine/audio/mixer_backend.h)：设备与轨道的创建、复用和销毁，与内置启动音频共享后端。
- [sound_playback_scheduler.cpp](../../../../engine/audio/sound_playback_scheduler.cpp)：预约与活跃实例、并发、冷却和句柄状态。
- [music_playback_controller.cpp](../../../../engine/audio/music_playback_controller.cpp)：单路音乐切换与停止状态机。
- [sound_playback_scheduler_tests.cpp](../../../../tests/audio/sound_playback_scheduler_tests.cpp)、[audio_fade_tests.cpp](../../../../tests/audio/audio_fade_tests.cpp)：调度与渐变回归。自动化通过不代表真实音频设备试听验收。

- [audio_failure_contract_tests.cpp](../../../../tests/audio/audio_failure_contract_tests.cpp)：注入后端错误、失败回滚、预约诊断与无场景全局 UI 提示音。
