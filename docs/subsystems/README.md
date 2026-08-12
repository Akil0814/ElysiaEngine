# 子系统文档

## 当前实现参考

- [UI](ui/README.md)：保留模式 UI、布局、焦点、窗口表面、样式和控件参考。
- [Input](input/README.md)：Raw Input、Action Mapping、Gameplay Input 和场景分发。
- [资源加载与 JSON](resources/README.md)：内容注册表、manifest、实体资源、动画和特效配置。
- [运行时配置](runtime-config.md)：AppConfig、UserConfigService 和 ConfigService。
- [音频服务](audio.md)：资源查找、播放调度、并发、冷却和音量。
- [Camera](camera.md)：相机槽位、控制器、跟随、震屏和坐标投影。
- [存档服务](save.md)：类型化存档、可靠写入、恢复与错误模型。
- [Engine Testbed](testbed.md)：跨子系统运行时实验场景的边界。
- [Development Overlay](development-overlay.md)：可选 Dear ImGui 开发覆盖层、输入捕获、面板生命周期与 Physics Inspector 示例。

## 实现指南

- [Physics](physics/README.md)：物理与 Gameplay 碰撞的当前审计、目标架构、逐类/逐函数契约、算法和实施路线。该目录同时包含尚未落地的目标设计，不能当作当前 API 清单。
