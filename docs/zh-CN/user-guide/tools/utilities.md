# 常用基础工具

本页介绍游戏代码可直接使用的向量、矩形、颜色、随机数与日志接口。计时器的推进、回调与暂停规则见[时间与计时器](../systems/time-and-timers.md)。这些值类型与工具不会自行注册到场景。

## Vector2：方向与距离

以下函数可放在普通游戏逻辑中，速度使用世界单位/秒，delta 使用秒。

```cpp
#include "engine/core/geometry/vector2.h"

elysia::core::Vector2 move_toward(elysia::core::Vector2 from,
    elysia::core::Vector2 to, float speed, double delta) {
    const auto offset = to - from;
    const auto distance = offset.length();
    if (speed <= 0.0f || delta <= 0.0) return from;
    const auto step = speed * static_cast<float>(delta);
    if (distance <= step) return to;
    return from + offset.normalized() * step;
}
```

Vector2 保存 float x/y，提供 length、length_squared、distance_to、distance_squared_to、direction_to、dot 和 cross。两个向量相乘是点积，不是逐分量乘法。`normalized()` 与 `normalize()` 都返回新向量；修改原值用 `normalize_in_place()`，近零向量会归零并返回 false。近似比较使用 `nearly_equals()`，不要用浮点精确相等判断运动是否结束。调用方保证输入数值有限。

## Rect：范围与相交

```cpp
#include "engine/core/geometry/rect.h"

// 放在区域判定函数中；参数必须采用同一种坐标系。
bool within_area(elysia::core::Vector2 point) {
    const elysia::core::Rect area{0, 0, 100, 80};
    return area.contains_half_open(point);
}
```

Rect 的构造顺序是 x、y、width、height；负宽高钳制为零，不表示反向矩形。两个端点使用 `from_points`，中心与尺寸使用 `from_center`。位置、尺寸、中心和边缘通过相应 getter 读取。

`contains(point)` 包含边界，`contains_half_open(point)` 排除右边与下边；对正常非空矩形，`intersects` 要求有正面积交叠，`touches_or_intersects` 包含边缘接触。需要排除退化矩形时先检查 `is_empty()`。`intersection` 取交集，`merged` 取包围矩形，`translated` 和 `expanded` 返回新矩形。它们只是几何运算，不进行物理过滤、事件或刚体响应。

## Color 与预定义颜色

```cpp
#include "engine/core/render/color.h"
#include "engine/core/render/colors.h"

// 在设置绘制颜色的函数中使用。
constexpr elysia::core::Color translucent_red{255, 0, 0, 128};
constexpr auto foreground = elysia::core::colors::white;
```

Color 使用 0–255 的 RGBA 通道，默认白色且不透明，省略 alpha 时为 255。`colors` 提供 white、black、transparent 等常量；它不是主题管理器。不要把 0–1 的浮点颜色直接当作这些整数通道。

## RandomGenerator

随机生成器是调用方持有的普通对象。将它保存在负责随机序列的游戏流程或对象中，不要为每次抽样都重新创建同一个固定种子的生成器。

```cpp
#include "engine/tools/random_generator.h"
#include <array>

// 例如关卡初始化时创建并保存 rng；固定种子便于重现问题。
elysia::tools::RandomGenerator rng{12345};
int count = rng.int_inclusive<int>(1, 3);
double offset = rng.real(-10.0, 10.0);
bool rare = rng.chance(0.1);
std::array<int, 3> rewards{10, 20, 30};
int reward = rng.pick(rewards); // 复制抽到的值。
```

| 方法 | 边界与错误 |
| --- | --- |
| `int_inclusive(min, max)` | 两端包含，min 大于 max 时抛 `std::invalid_argument` |
| `real(min, max)` | 左闭右开，端点必须有限且 min 小于 max |
| `chance(p)` | p 为有限的 [0,1] 值，0 必假、1 必真，越界抛异常 |
| `pick(values)` | 从非空随机访问且可取大小的范围抽样；空范围抛异常 |

`pick()` 返回容器元素引用，示例将其复制；保存引用时必须考虑原容器的生命和扩容。来自玩家或配置的范围与概率应先验证，不要用异常作为每帧正常控制流。

无参构造使用熵种子；显式种子便于在相同调用流程下复现结果，不代表自动获得联机确定性。`set_seed()` 重置随机引擎并清除固定值注入。测试用 `set_fixed_values()` 注入的是原始随机整数序列，不是每个高级接口的最终结果；序列耗尽后继续使用引擎，拒绝采样等过程也可能消耗多个值。

## 日志

```cpp
#include "engine/tools/logger.h"

// 放在有业务上下文的函数中：
ELYSIA_LOG_INFO("gameplay", "Spawned enemies: " << 3);
ELYSIA_LOG_WARN("inventory", "Reward skipped because inventory is full.");
```

第一个参数是分类，第二部分支持流式表达式，不是 printf 占位符格式。可使用 `ELYSIA_LOG_DEBUG`、`ELYSIA_LOG_INFO`、`ELYSIA_LOG_WARN`、`ELYSIA_LOG_ERROR`；`ELYSIA_LOG` 是 Info 的便利入口。分类保持稳定，记录定位问题需要的对象标识、操作和错误原因，避免无条件每帧输出大量日志。

`ELYSIA_LOG_TERMINATING` 记录 Terminating 级别日志，本身不发出退出请求。应用负责日志设施的生命周期，普通游戏对象不重复初始化或关闭 Logger。

打印 Error 不会自动恢复操作、切换场景或终止应用。调用方仍要检查服务返回值并决定停止操作、提示用户或进入[应用错误流程](../builtin-scenes/application-failure.md)。日志也不替代 `std::expected` 等接口的错误处理。

## 参考

- [随机数接口](../../../../engine/tools/random_generator.h)、[日志接口](../../../../engine/tools/logger.h)
- [返回使用指南](../README.md)
