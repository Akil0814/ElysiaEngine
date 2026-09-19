# 输入与控制器接口迁移

本轮不保留兼容别名或旧入口转发；仓库内示例和测试已迁移。

| 旧接口／行为 | 新接口／行为 |
| --- | --- |
| DevelopmentInputCapture 与旧头文件 | InputCapture，包含 engine/input/input_capture.h；位检查使用 captures_input |
| InputSystem::current_device、InputDeviceTracker | 移除；采集按事件分类，UI 自行维护活动设备 |
| InputSystem::end_frame、GamepadInputTranslator::reset | 移除空生命周期或无用入口 |
| 原始／UI 滚轮整数 | float，消费者保留小数，整数来源显式转换或使用浮点字段 |
| bind_target、unbind_target、replace_input_map 的布尔成功判断 | ControllerOperation；明确检查 Pending、Succeeded、Failed |
| 设备绑定／解绑／转移／替换的 bool | expected<void, InputBindingError>；检查或记录 error |
| ControllerService::remove 的 bool | expected<void, ControllerError> |
| 捕获过滤隐式推进状态 | observe 更新门控，const filter 只读取 |

配置调用在回调内可能排队，不能用 `!succeeded()` 直接认定失败；需要等待的调用方保存结果，在更新阶段处理终态。仅允许同步执行的初始化流程应要求 Succeeded，并将 Pending 视为调用时机不满足要求。多目标相机示例保存换绑请求，仅在成功后改变主目标。

取消旧目标前的绑定校验失败保留旧绑定；取消回调执行后才发现目标失效时保持解绑。取消不是按键释放，不应触发松键攻击。每次失焦只通知一次 FocusLost，暂停只通知 Paused，持续不可用不重复通知。

本轮自动化测试使用合成输入。实体手柄拔插、触控板高精度滚动及开发覆盖层实机体验仍应执行硬件验收，不以合成测试替代。
