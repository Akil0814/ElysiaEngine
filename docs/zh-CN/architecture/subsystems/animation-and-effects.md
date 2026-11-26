# 动画与特效架构

本文解释共享动画定义、独立播放实例、特效分发和场景所有权。游戏接入见[动画与特效指南](../../user-guide/systems/animation-and-effects.md)，内容注册链路见[资源加载](resources/README.md)。

<a id="animation-service-architecture"></a>
## 动画服务架构

注册表保存共享定义，创建接口返回带独立播放状态的动画实例。

```mermaid
flowchart TB
  Loader["GameContentLoader"]
  Resources["ResourceService / ResourceManager<br/>持有图集和纹理"]
  Manager["AnimationManager<br/>持有 AnimationDefinition 注册表"]
  Service["AnimationService / ELYSIA_ANIMATIONS"]
  Caller["游戏对象 / AnimationEffectFactory"]
  Instance["Animation<br/>帧索引、计时、循环与回调状态"]
  Loader -->|图集就绪后 find_atlas| Resources
  Loader -->|register_animation：借用图集| Manager
  Manager -.->|定义中图集指针为借用| Resources
  Caller -->|find_definition / create_animation| Service
  Service -->|转发定义查询| Manager
  Service -->|从定义创建独立实例| Instance
  Caller -->|接收 unique_ptr 并负责推进| Instance
  Instance -.->|借用图集，不拥有纹理| Resources
```

find_definition 返回借用，create_animation 返回调用方拥有的 unique_ptr。定义和实例都借用资源图集；重载前必须停止使用旧实例和定义。缺失定义时创建返回 nullptr，不触发按需加载。

源码：[animation_service.cpp](../../../../engine/animation/animation_service.cpp) · [animation_manager.h](../../../../engine/animation/runtime/animation_manager.h) · [animation.h](../../../../engine/animation/animation.h)

<a id="effect-service-architecture"></a>
## 特效服务架构

屏幕特效与 CPU 粒子组由 Manager 的独立运行时持有，动画和飘字特效对象转交活动 Scene 持有。

```mermaid
flowchart TB
  Caller["游戏 / UI"]
  Service["EffectService / ELYSIA_EFFECTS"]
  Loader["GameContentLoader"]
  Manager["EffectManager<br/>定义注册表与请求分发"]
  Screen["ScreenEffectRuntime<br/>持有屏幕特效与句柄状态"]
  Particles["ParticleRuntime / ParticleEffect<br/>粒子组、连续数组与组级句柄"]
  Factories["AnimationEffectFactory / FloatingNumberEffectFactory"]
  Dependencies["AnimationService / FontResolver<br/>动画实例与数字字形缓存依赖"]
  Scene["活动 Scene<br/>持有生成的 AnimationEffect / FloatingNumberEffect"]
  Lifecycle["SceneManager / Scene<br/>绑定、更新、渲染与退出清理"]
  Caller -->|请求、停止、取消与状态查询| Service
  Service -->|转发请求与屏幕特效控制| Manager
  Loader -->|register_animation_effect| Manager
  Manager -->|持有；分发屏幕请求| Screen
  Manager -->|持有；分发粒子请求| Particles
  Manager -->|持有；创建场景对象特效| Factories
  Factories -->|创建动画 / 借用字体解析能力| Dependencies
  Manager -.->|借用活动场景，转交对象所有权| Scene
  Lifecycle -->|绑定活动场景并协调屏幕特效| Manager
  Lifecycle -->|推进对象生命周期| Scene
```

EffectManager 借用活动场景和渲染、字体依赖；缺少运行时或定义时返回明确失败。屏幕特效不跨场景退出；场景对象特效遵循对象生命周期，不能将 EffectService 视为资源加载器。

源码：[effect_service.cpp](../../../../engine/effects/effect_service.cpp) · [effect_manager.h](../../../../engine/effects/runtime/effect_manager.h) · [effect_manager.cpp](../../../../engine/effects/runtime/effect_manager.cpp)

## 粒子运行时

纹理就绪后，GameContentLoader 通过 EffectManager 校验并注册清单里的粒子定义。游戏侧通过 EffectService 请求和控制实例，实例创建时复制参数并借用纹理。ParticleRuntime 按 slot/generation 管理组级句柄，每组 ParticleEffect 持有连续粒子数组和预分配绘制缓存，不注册 GameObject，也不产生逐粒子回调。

SceneManager 在场景对象更新前推进粒子，使用缩放后的场景 delta，并跳过创建帧和暂停帧。Scene 在各个世界深度层的普通对象绘制后调用粒子批量绘制，包括没有普通对象的层；同层组按创建顺序提交，每组使用一次 SDL_RenderGeometry。绘制按粒子寿命计算顶点颜色和大小，应用相机投影，并在成功和失败时恢复借用纹理的混合模式。

停止仅结束发射，取消立即回收实例。场景解绑、故障恢复、关闭和内容清理均清除粒子；清理发生在纹理释放之前。碰撞、伤害和角色生命周期仍由游戏负责，发射位置由值参数更新。

相关入口：[服务架构总览](../services.md) · [架构首页](../README.md)。
