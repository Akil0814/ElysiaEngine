# 物理单位、命令与状态快照

本文说明当前 Box2D 后端的公开约定。游戏层应通过引擎命令修改物理状态，再通过快照读取已提交结果。

## 1. 空间单位与方向

每个世界使用一个不可变的 `units_per_meter` 比例，默认 100。EU 表示引擎世界单位；时间为秒，角度为弧度，质量为千克。`SurfaceDensity` 表示 kg/m²，不另设独立的单位倍率。

设 `S = units_per_meter`，输入到 Box2D 时按下表转换：

| 量 | 公开单位 | 输入换算 |
| --- | --- | --- |
| 位置、长度 | EU | 除以 S |
| 速度 | EU/s | 除以 S |
| 加速度、重力 | EU/s² | 除以 S |
| 力 | kg·EU/s² | 除以 S |
| 线性冲量 | kg·EU/s | 除以 S |
| 力矩 | kg·EU²/s² | 除以 S² |
| 角冲量 | kg·EU²/s | 除以 S² |
| 绕质心转动惯量 | kg·EU² | 除以 S² |
| 角度、角速度 | rad、rad/s | 不变 |
| 时间、质量、密度 | s、kg、kg/m² | 不变 |

输出使用反向转换。关节锚点、反作用力和马达力矩遵循同一张表。弹簧频率为 Hz，阻尼比、摩擦和恢复系数无量纲；线性与角阻尼系数的单位是秒的倒数。

坐标保持 X 向右、Y 向下，正旋转、角速度和力矩在屏幕上表现为顺时针。适配层不翻转 Y 轴；需要角度制的渲染接口只在绘制边界转换。

## 2. 创建、修改与读取

`PhysicsWorld::register_object(owner, body_definition, collider_definitions)` 复制定义并返回稳定的引擎句柄。`collider_id(handle, index)` 查询形状身份；创建定义本身不包含存活对象的标识。

`PhysicsParticipant` 提供创建描述并接收世界与句柄绑定，不公开可写模拟状态。`PhysicsStepParticipant::fixed_update` 在每个模拟步提供控制。

修改状态使用 `set_velocity`、`set_angular_velocity`、`apply_force`、`apply_impulse`、`apply_torque`、`apply_angular_impulse`、`set_transform` 或 `teleport_object`。力与冲量的可选作用点为世界坐标，省略时作用于质心。直接改写已注册 `GameObject` 的位置不等于提交物理命令。

`body_state`、`render_pose` 和 `joint_state` 返回快照，无效句柄返回空 `optional`。游戏规则使用已提交状态，绘制使用插值姿态。`update_collider` 替换复制的形状描述，`set_collider_enabled` 只改变启用状态；相同描述不会重建形状。

## 3. 质量与关节

`BodyDefinition::mass_properties` 是三种互斥配置：默认 `DensityMass{}` 根据以米计的形状面积与密度计算质量数据；`ExplicitMass{mass}` 保留密度质心，并按总质量同比缩放惯量；`BodyMassData{mass, local_center, rotational_inertia}` 独立指定质量、局部质心与绕质心惯量。传感器默认不贡献质量，只有 `sensor_contributes_mass` 开启时才参与密度计算。

`PhysicsWorld::set_mass_properties(handle, properties)` 完整替换配置；传入 `DensityMass{}` 恢复密度计算。`mass_properties(handle)` 返回已执行的配置，`body_state(handle)` 返回实际质量、有效惯量与 `local_center` / `world_center`。两种查询在原生创建尚未完成或身份失效时返回空。`PhysicsParticipant` 提供同名配置读写快捷接口。

仅 Dynamic 接受质量覆盖；Static / Kinematic 只接受 `DensityMass`。覆盖质量必须有限且为正，质心有限，惯量有限且非负；换算结果及所需倒数必须可表示。零惯量不响应力矩；固定旋转的有效惯量始终为零，但配置保留请求值。碰撞体替换、密度或质量贡献变化、启停后会重新应用当前配置，完整自定义数据不会被形状自动计算覆盖。

运行时改变质心保留刚体原点位姿、角速度和原点瞬时速度；引擎按 `v_new = v_old + angular_velocity × (world_center_new - world_center_old)` 调整质心线速度，不保证动量或能量守恒。关节局部锚点不变；已启用刚体被唤醒，禁用刚体仍禁用。回调中的配置按值复制进入 FIFO；`true` 表示校验通过并接受，执行前再次校验身份和当时形状，失效或不再有效的命令跳过。队列分配异常沿用现有异常契约，入队失败不修改配置。

惯量单位为 kg·EU²，参考点为质心；这与部分旧 Box2D 形状质量数据绕局部原点的惯量不同。迁移说明和转杆示例见[独立质量属性回执](../../integration/006-explicit-inertia-receipt.md)。

距离关节提供刚性静止长度，或带频率与阻尼比的弹簧。旋转关节提供局部锚点、参考角、角度限位、马达速度和最大力矩；相连刚体默认不互相碰撞。原生约束刚度有限，极大马达力矩可能越过名义角度限位。

摩擦、接触、休眠与 CCD 使用 Box2D 行为，不保证保留旧后端的数值轨迹。

## 4. 调试绘制

默认碰撞体轮廓与普通绘制使用相同的插值位置和最短角度旋转。绿色表示清醒刚体，蓝色表示休眠或静态刚体，紫色表示传感器；禁用刚体不显示。关节锚点也使用插值姿态。

Inspector 的 `Native AABB (physics step)` 单独显示黄色轴对齐原生边界。它不是旋转碰撞体轮廓，也不是 CCD 扫掠路径。`Previous / current physics poses` 用灰色显示前一姿态、白色显示最新模拟姿态；两项都有独立类别开关。

接触点、法线和速度矢量表示最近完成的物理步，刻意不做插值。延迟修改提交后或调试模式变化时会刷新捕获，暂停时也一样。没有物理步的渲染帧仍更新碰撞体的插值系数。

## 地形与目标命令

Chain 顶点、ghost 与 Segment 端点也使用 EU，正面为局部 `(dy,-dx)` 随刚体旋转。Chain 子段不可通过 Collider 修改，身份由 `chain_collider` 与 `chain_segment_info` 查询。`set_target_transform(handle, pose, dt)` 仅针对运动学刚体，每步提交一次；不提交时保留当前速度，同目标或原生休眠阈值内目标清除旧速度。

八类 `JointState.details` 提供类型化设置与测量，公共 `first`、`second` 标识两端；Filter 无设置，反作用力／扭矩为零。Motor 的位置目标在第一端局部坐标中，Mouse 的目标在世界坐标中，二者误差按各自坐标表达。

Filter 与 `collide_connected` 遵循内置 Box2D 的实体接触规则；它们不屏蔽原生 Sensor 重叠，也不改变无状态查询。需要排除传感器或查询目标时，使用 Collider 过滤器或 `CollisionQueryFilter`。

## 类型化物理调试快照

`PhysicsDebugCapture` 新增 `Chains` 和 `Targets`。`PhysicsDebugJoint` 拥有 `JointState`、两端前后姿态、局部锚点、局部轴和参考角；`PhysicsDebugChain` 拥有实际顶点、子段 ColliderId、ghost、owner 与前后姿态；`PhysicsDebugTarget` 保存命令执行时姿态、请求／生效目标、时间步、待模拟标记和目标轮廓。`PhysicsDebugSnapshot` 包含 `chains`、`targets` 和 `units_per_meter`，不借用世界或原生数据。绘制可在世界销毁后继续消费已有快照。

捕获按类别选择，关节基础与细节共用 `Joints`；关闭细节不构建对应 Chain／目标轮廓。捕获失败保留上一完整快照及配置，异常向上传播，不修改原生运动状态。目标命令本身只更新固定大小诊断记录。开关、插值及记录生命周期见[调试绘制](../../user-guide/tools/debug-draw.md)。

`PhysicsDebugSnapshot::bodies` 随 Shapes 或 BroadPhase 捕获，包含每个可见刚体的已提交质量配置与 `BodyState`，无碰撞体的刚体也包含在内。Inspector 使用这些拥有数据的快照；数值表示捕获时的物理状态，不做插值。
