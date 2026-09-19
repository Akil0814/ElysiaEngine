# 时间与计时器

时间接口使用秒。`ELYSIA_TIME` 提供应用时间，`Timer` 是由调用方驱动的普通工具，两者不会因为同时存在而自动连接。

## 选择时间来源

| 接口 | 含义 |
| --- | --- |
| `raw_delta()` | 未乘全局倍率的本帧时间增量 |
| `delta()` | 本帧原始增量乘以全局 `time_scale()` |
| `total_time()` | 累积原始时间 |
| `scaled_total_time()` | 累积缩放后的时间 |
| `frame_count()` | 已开始的帧数，不是物理 tick 数 |

应用每帧调用 `begin_frame()`；游戏不要再次调用它或随意 `reset()` 全局时间。正常场景更新收到的是缩放后的 `delta`。需要不受慢动作影响的功能可主动选择 `raw_delta()`，但调用该功能的更新若已被暂停跳过，原始时间本身不会替它恢复调度。

```cpp
#include "engine/core/time.h"

// 在游戏流程中启用慢动作；效果结束时恢复项目期望的倍率。
ELYSIA_TIME->set_time_scale(0.5);
// ……慢动作结束……
ELYSIA_TIME->set_time_scale(1.0);
```

倍率在 `begin_frame()` 时参与计算，因此帧中修改从下一帧生效。零倍率停止时间推进，但输入、渲染和收到零 delta 的更新仍可能执行，不等于场景暂停。不要用零时间增量替代明确的输入禁用策略。

## 物理、暂停与对象倍率

场景把缩放后的 delta 交给物理世界累积。全局慢动作会减少单位真实时间内的固定步数量，固定步长本身不变；一帧可执行零次或多次。固定步回调不要再乘全局倍率。单次推进有最大步数限制，过大的积压会丢弃，不能用超大倍率承诺无限精确快进。

`Scene::pause()` 会停止该场景物理推进，并按对象的暂停策略筛选普通更新与输入；不会停止全局时钟。`GameObject::set_time_scale()` 只是对象级倍率，普通场景调度不会自动替对象应用它。需要时在对象自己的逻辑中使用 `scaled_delta(delta)`；它不会自动改变刚体模拟速度。

## 手动驱动 Timer

以下类可作为游戏对象的成员，在对象自己的更新中调用 `update(delta)`。它不自动注册到场景。成员计时器随拥有者销毁，避免独立长生命周期回调继续访问失效对象。

```cpp
#include "engine/tools/timer.h"

class Cooldown
{
public:
    Cooldown()
    {
        timer_.set_wait_time(2.0);
        timer_.set_one_shot(true);
        timer_.set_on_timeout([this] { ready_ = true; });
    }
    Cooldown(const Cooldown&) = delete;
    Cooldown& operator=(const Cooldown&) = delete;
    void start() { ready_ = false; timer_.restart(); }
    void update(double delta) { timer_.update(delta); }
    bool ready() const { return ready_; }
private:
    bool ready_ = false;
    elysia::tools::Timer timer_;
};
```

`set_wait_time()` 设置间隔而不自动重启；`restart()` 清空累计时间和已触发标记并解除暂停。`pause()` 保留进度，`resume()` 恢复；单次计时器触发后会暂停，再开始一轮应使用 `restart()`。

循环计时器 `set_one_shot(false)` 在大 delta 下可能连续触发多次以消耗累计间隔。非正等待时间或非正 delta 不推进。回调中重启、暂停或修改等待时间会中止当前这一轮继续补触发；不要在回调中直接销毁正在执行的 Timer 所有者。

## 参考

- [Time](../../../engine/core/time.h)、[Timer](../../../engine/tools/timer.h)
- [核心更新概念](core_concepts.md)、[返回使用指南](README.md)
