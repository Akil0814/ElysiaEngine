# 中文文档评估与问题清单

评估日期：2026-10-21。源码基线：`07e9d46`（`update docs CN`）。本报告是该基线下的评估记录，不作为现行 API 参考；后续修复应以源码和测试为依据，并保留本报告的历史属性。

中文文档已形成较完整的使用指南、接口参考、架构、贡献指南和设计提案体系。当前主要问题是迁移后的说明没有同步更新，以及不同文档重复维护同一契约。中文本地链接检查通过，纳入文档编译目标的示例也通过了编译，但这两项检查不能发现全部事实错误。

本次确认 **7 项内容问题：P2 6 项、P3 1 项**，另列出 **3 项维护建议**。没有在中文范围内确认需要列为 P0/P1 的问题。优先修复资源模块现状、音频增益模型、物理接口和测试环境说明，再处理统计、调试面板及返回类型。

## 1. 范围与判定方法

### 1.1 被评估文档

| 范围 | 文件数 | 主要内容 |
| --- | ---: | --- |
| 中文总入口与根目录中文 README | 2 | 项目介绍、分类入口 |
| `user-guide/` | 30 | 构建运行、游戏接入、各子系统使用 |
| `reference/` | 20 | UI、输入、物理与资源格式契约 |
| `architecture/` | 25 | 子系统职责、生命周期、内部机制与历史验证 |
| `contributing/` | 5 | 规范、测试、统计、文档维护 |
| `design/` | 3 | 路线图及尚未落地的网络设计 |
| 合计 | **85** | **9,100 行** |

上述数字对应报告新增前的 84 篇 `docs/zh-CN/` 文档和根目录 `README.zh-CN.md`，不包括本报告。中文文档中的源码、配置和构建文件链接也作为事实核对入口。

英文、日文文档、上游第三方说明和 Word/PDF 归档不纳入本报告的问题统计。此前全量评估中的英日首页断链与英文 ImGui 集成说明问题因此没有沿用到这里。

### 1.2 检查内容与边界

- 对范围内文档执行本地链接、标题锚点和 Markdown 代码围栏检查。
- 盘点 689 处本地 Markdown 链接、130 个 C++ 块、29 个 JSON 块和 5 个 Mermaid 块。
- 对应用、场景、对象、输入、控制器、UI、物理、相机、资源、音频、动画特效、时间、本地化、配置、存档和开发工具的关键说明交叉核对源码或实际配置。
- 区分当前行为、假设示例、设计提案及历史验收记录；不把明确标记的旧设计当作现行 API 错误。
- 区分确定事实错误与维护建议；不把未编译的概念片段直接判为无效代码。

没有重新执行完整 CTest、Release/ImGui OFF 构建、其他平台构建、真实窗口与手柄操作、音频试听或外链联网验证。源码交叉核对不是每个 API 的运行验收，JSON 语法通过也不代表其引用的素材全部存在。

### 1.3 严重程度

| 等级 | 本报告中的含义 |
| --- | --- |
| P0 | 需要立即处理的严重问题 |
| P1 | 阻断主要阅读或接入流程的问题 |
| P2 | 会误导调用、实现维护、资源准备或测试操作的明确错误 |
| P3 | 局部描述精度问题，读者仍可通过相关源码纠正 |
| 建议 | 降低未来漂移或改善验证的方向，不计入已确认错误数量 |

## 2. 问题总表

| 编号 | 等级 | 问题 | 主要文档 | 建议顺序 |
| --- | --- | --- | --- | ---: |
| CN-01 | P2 | 音频架构仍使用整数音量上限及取整公式 | [音频架构](../architecture/subsystems/audio.md) | 2 |
| CN-02 | P2 | 资源说明把旧模块写成仓库当前配置 | [实体内容模块](../reference/resources/entity-content.md)、[动画与特效格式](../reference/resources/animation-and-effects.md)、[资源架构](../architecture/subsystems/resources/README.md) | 1 |
| CN-03 | P2 | 物理参考表列出已移除的 `advance` | [物理参考](../reference/physics/README.md) | 3 |
| CN-04 | P2 | 语言特性统计过期且一致性检查失败 | [语言特性盘点](cpp-language-features.md) | 5 |
| CN-05 | P2 | 无显示环境测试条件说明不完整 | [测试指南](testing.md) | 4 |
| CN-06 | P2 | Physics Inspector 功能清单仍是旧后端指标 | [开发面板指南](../user-guide/tools/development-panels.md) | 6 |
| CN-07 | P3 | 玩家设备管理接口的返回类型概括不准确 | [输入架构](../architecture/subsystems/input/architecture.md) | 7 |

## 3. 已确认问题的详细依据

### CN-01 P2 音频架构使用了过期的音量计算模型

**位置**：[音频架构](../architecture/subsystems/audio.md) 第 231、251、254 行；对照[音频使用指南](../user-guide/systems/audio.md)的“音量模型”章节。

**当前描述**：音乐和音效的有效音量都乘以 `MIX_MAX_VOLUME`，随后说明最终 SDL 音量会统一向下取整。

**源码事实**：[audio_service.cpp](../../../engine/audio/audio_service.cpp) 第 216–220 行、292–296 行计算浮点比例，并将结果直接传给 `MIX_SetTrackGain`。当前实现没有上述整数上限因子或向下取整步骤。使用指南已经采用浮点增益模型，因此两份中文说明存在直接冲突。

正确的关系为：

```text
music_gain = master_volume / 100 × music_volume / 100 × music_fade_gain
sound_gain = master_volume / 100 × sound_volume / 100 × group_volume / 100 × instance_fade_gain
```

以上除法按浮点比例计算，音量设置范围仍为 `0..100`，渐变增益仍独立参与相乘。

**影响**：修改音频后端、排查低音量或渐变行为时，维护者可能按旧版整数通道 API 的模型理解当前轨道增益；也难以判断两份说明哪一份有效。

**建议修改**：更新架构中的两条公式和取整说明；公共音量契约选择一处维护，另一处通过链接引用，架构保留轨道、调度器与后端的分工。

**验收条件**：两份中文文档与 `apply_music_volume`、`apply_sound_channel_volume` 保持一致；作为当前行为的说明不再依赖 `MIX_MAX_VOLUME`，也不再声称存在整数取整步骤。

### CN-02 P2 资源文档的当前模块清单与仓库配置不一致

**位置**：

- [实体内容模块](../reference/resources/entity-content.md) 第 5 行、“当前模块划分”第 101–109 行。
- [动画与特效 JSON](../reference/resources/animation-and-effects.md) 第 163 行。
- [资源加载管线](../architecture/subsystems/resources/README.md) 第 18–24 行。

**当前描述**：这些位置将 `characters`、`character_effects`、`enemies` 写成仓库当前模块，并写明角色特效目前通过 `character_effects` 加载。相邻 JSON 示例沿用 `configs/character/...` 和 `textures/character/...` 路径。

**配置事实**：[assets/content_registry.json](../../../assets/content_registry.json) 的 `manifests.additional` 当前只有：

```json
{
  "ryougi_sample": "configs/examples/ryougi/content_manifest.json"
}
```

该片段只摘录 `additional` 的内容，不是可以直接替代整个注册表的根结构。

[实际模块清单](../../../assets/configs/examples/ryougi/content_manifest.json) 只声明 `animations` 能力，`key_namespace` 为空；模板与资源根分别位于 `configs/examples/ryougi/` 和 `textures/examples/ryougi/`。旧的 `configs/character/...` 目录不存在。

**影响**：读者按“当前模块”查找素材、模板和特效键会找不到对应内容；维护者也可能把旧项目素材布局误认为引擎固定约束。模块样例被移除不等于通用模块加载器移除了 Texture、Audio 或 Effect 能力。

**建议修改**：

1. 当前模块表改为实际的 `ryougi_sample`，列明空命名空间和动画能力。
2. 资源架构图使用当前注册表，或明确将多模块分支标为通用示意。
3. 将保留的旧复合模块示例标为假设项目配置，说明相应文件需要调用方自行提供。
4. 通过真实模块清单、实体清单、布局和动画配置组成一条可在仓库中定位的阅读路径。
5. 不把假设模块的 `RyougiShiki.effect.*` 写成默认运行时一定存在的键。

**验收条件**：所有使用“当前”“仓库配置”的资源模块说明都能对应实际 JSON；示意格式与默认样例清晰区分；不为迎合旧文档重新引入已经移除的项目素材。

### CN-03 P2 物理接口总览仍列出 advance

**位置**：[物理参考](../reference/physics/README.md) 第 18 行，“固定步进与渲染插值”能力表。

**当前描述**：该行把 `PhysicsWorldConfig`、`advance`、`render_pose` 列为主要接口。

**源码事实**：[physics_world.h](../../../engine/physics/physics_world.h) 第 80–81 行提供 `step(double fixed_delta_seconds)` 与 `finalize_frame(double interpolation_alpha)`，没有 `advance`。固定步累计由场景持有的 `FixedStepRuntime` 负责。该参考正文第 5 行已经描述新调度方式，表格没有同步。

**影响**：读者查阅能力表时得到不存在的调用入口，也可能误以为物理世界仍直接管理场景固定步累计器。

**建议修改**：表格按场景固定步调度、实际模拟、帧末插值和渲染快照分别列出职责。继续强调普通游戏代码依赖场景自动驱动，不应因接口出现在参考表中就重复手动推进。

**验收条件**：当前接口总览不再列出 `advance`；与物理指南和固定步架构一致。网络设计文档中明确标为历史审计的旧 `PhysicsWorld::advance()` 可以保留，不应机械地全局替换。

### CN-04 P2 语言特性盘点无法复现当前源码统计

**位置**：[语言特性盘点](cpp-language-features.md) 第 20 行及“维护约定”；[测试指南](testing.md)第 55–59 行规定的检查命令。

**当前描述**：统计块称按当前工作树生成，共扫描 759 个第一方 C++ 文件，并提供 `-Check` 作为一致性验证。

**复核结果**：运行以下命令失败：

```powershell
pwsh -NoProfile -File scripts/audit_cpp_language_features.ps1 -Check
```

错误为：

```text
Unclassified std:: symbols: hex, locale, numpunct, uppercase
```

[统计脚本](../../../scripts/audit_cpp_language_features.ps1) 在未分类符号检查阶段终止；按文档声明的 `engine/`、`game/`、`tests/` 和 `.h`、`.hpp`、`.cpp` 范围枚举，当前实际为 763 个文件。相同基线下 `-SelfTest` 通过，但它不能证明统计块仍与源码一致。

**影响**：贡献者按指南也无法完成规定检查；“当前统计”数字已经失去可复现依据。由于脚本先在分类阶段退出，本次没有把表中每项旧数值与新统计逐一比较，不能仅凭文件数差异推断所有行都错误。

**建议修改**：先为新增符号确定版本归属并完善分类，必要时补充自测；随后执行 `-Update`，审阅生成变化，再运行 `-Check`。不要直接把 759 改成 763，也不要跳过分类断言来得到通过结果。

**验收条件**：`-SelfTest` 和 `-Check` 同时通过；生成块、扫描范围和代表性源码链接与当前工作树一致。

### CN-05 P2 无显示环境测试说明遗漏其他标签下的 GPU 依赖

**位置**：[测试指南](testing.md) 第 32 行；对照[构建指南](../user-guide/build-and-run.md) 第 86 行。

**当前描述**：贡献测试指南建议无显示环境用 `-LE '^gpu$'` 排除 GPU 测试，但没有提示其他标签中仍有依赖 GPU 的用例。构建指南已经明确写出部分 `application` 生命周期测试也需要 GPU。

**源码事实**：[tests/CMakeLists.txt](../../../tests/CMakeLists.txt) 第 626–634 行把主 `application_lifecycle_tests` 标为 `gpu`，但各独立执行分支只标为 `application`。在本次 ImGui ON 配置中，排除 `gpu` 后仍保留 12 个分支：2 个 `manager_*` 分支不初始化 GPU，其余 10 个会进入应用 GPU 初始化路径。

[application_lifecycle_tests.cpp](../../../tests/application/application_lifecycle_tests.cpp) 第 102–145 行展示了这一分支差异；应用本身使用 GPU renderer 初始化流程。

**影响**：读者可能把“排除 gpu 标签”当成完整的无显示环境筛选方法，在 dummy/headless 环境运行剩余测试后遇到设备初始化失败，并误以为是引擎回归。

**建议修改**：

- 立即在贡献测试指南同步构建指南中的限制，并链接同一份设备要求说明。
- 后续按真实设备依赖补齐测试标签，使文档的筛选命令与 CTest 注册事实一致。
- 不要简单把所有 `application` 测试都视为 GPU 测试；保留可在无 GPU 环境运行的管理器和纯逻辑检查。

**验收条件**：使用 `ctest --show-only=json-v1` 核对筛选结果；若提供可直接执行的 headless 命令，按声明环境验证其保留的用例。本次只根据源码与注册表确认设备依赖，没有重新执行 dummy 环境测试。

### CN-06 P2 Physics Inspector 功能清单仍描述旧物理后端

**位置**：[开发面板指南](../user-guide/tools/development-panels.md) 第 124–127 行。

**当前描述**：面板提供 solver iterations、proxy、broad-phase、narrow test、Tile、CCD、solver，以及 pair、Tile candidate 等统计。

**源码事实**：[physics_inspector.cpp](../../../game/showcase/shared/physics_inspector.cpp) 第 38–79 行实际提供：

| 区域 | 当前显示内容 |
| --- | --- |
| 世界配置 | 固定步长、重力、最大补步、Box2D sub-steps |
| 固定步统计 | 已注册对象、碰撞体、接触、清醒刚体、关节、步耗时、丢弃固定步数 |
| 调试快照 | Shapes、Contacts、Velocities |

第 104 行之后的调试开关还包括 Native AABB、前一/当前物理姿态和关节锚点，指南没有完整同步。当前面板不再公开上述旧求解器内部指标。

**影响**：调试者会寻找不存在的统计，误把 Box2D 子步当成旧求解器迭代；也无法通过指南理解新调试开关。

**建议修改**：按实际面板和物理统计类型重写清单，说明 sub-steps 与场景补步数的区别，说明 Native AABB 是物理步边界、姿态历史用于插值诊断，并移除旧 CCD/求解器指标承诺。

**验收条件**：指南列出的每项统计和开关都能在当前面板源码中定位；说明与[物理单位和状态参考](../reference/physics/units-and-state.md)的调试约定保持一致。

### CN-07 P3 玩家设备管理的返回类型概括不准确

**位置**：[输入架构](../architecture/subsystems/input/architecture.md) 第 49–51 行。

**当前描述**：列出 `create_partition` 等操作后，统一写成返回 `expected<void, InputBindingError>`，又把 `apply_configuration` 描述成返回 `InputBindingError`。

**源码事实**：[local_player_registry.h](../../../engine/input/local_player_registry.h) 第 146、199 行分别为：

| 操作 | 实际返回类型 | 成功时的含义 |
| --- | --- | --- |
| `create_partition` | `std::expected<KeyboardPartitionId, InputBindingError>` | 返回新分区 ID |
| `apply_configuration` | `std::expected<void, InputBindingError>` | 批量配置已成功提交 |
| 更新、删除分区及设备绑定等修改操作 | `std::expected<void, InputBindingError>` | 修改成功；失败时读取错误 |

**影响**：读者可能忽略创建成功时必须保留的分区 ID，或把错误枚举与完整的操作结果类型混为一谈。相关头文件仍可直接查到，因此列为局部精度问题。

**建议修改**：把创建接口与纯修改接口分开描述，明确 `.error()` 的类型；批量配置使用完整的 `expected` 结果，不直接将错误类型写成函数返回类型。

**验收条件**：架构摘要中的返回类型、成功值和错误值与头文件一致；不改变现有代码契约。

## 4. 维护建议

以下建议不与前述 7 项内容问题合并计数。

### R-01 收敛重复维护的完整契约

音频指南与架构、配置指南与架构、物理指南与参考存在多段重复。此前扫描发现 28 组长度超过 100 字符的完全相同段落或片段，集中在这些中文文档；不完全相同的重复内容没有包含在这一数字中。

重复不一定是错误，但 CN-01 已显示同一模型在两处维护会发生漂移。[文档维护规则](documentation.md)要求完整字段与契约只维护一份。

建议按以下职责整理：

| 文档类型 | 保留内容 | 通过链接引用的内容 |
| --- | --- | --- |
| 使用指南 | 前提、完成任务的步骤、失败处理、简短示例 | 完整签名、字段、默认值和穷尽边界 |
| 接口参考 | 参数、返回结果、默认值、生命周期与完整调用契约 | 任务教程和实现推导 |
| 架构 | 所有权、内部协作、调度与设计约束 | 已集中维护的公共 API 表和完整配置块 |

可先处理音频，再处理 AppConfig/UserConfig 和物理接口。整理时保留理解上下文所需的摘要，并同时检查原有锚点与入站链接。

### R-02 明确中文链接检查的覆盖范围

[测试指南](testing.md)推荐无参数的链接检查命令；[检查脚本](../../../scripts/check_documentation_links.py)默认完整检查 `docs/zh-CN`，并额外检查指向该目录的入站链接。根目录中文 README 并不会因此自动获得全部链接检查。

本次显式加入 `README.zh-CN.md` 后，85 篇检查仍然为 0 错误，因此这里不是已发生的中文断链缺陷。建议贡献指南明确列出中文范围的完整命令：

```powershell
python scripts/check_documentation_links.py docs/zh-CN README.zh-CN.md
```

输出中应区分“中文文档通过”“全项目文档通过”，不能仅使用笼统的“所有文档链接通过”。后续若扩展检查器，HTML 图片、引用式 Markdown 链接和外链也应单独说明支持范围。

### R-03 按示例用途扩展编译验证

中文范围共有 130 个 C++ 块；当前文档示例目标从 29 篇文档中提取 42 个块，生成 30 个编译单元。其余块包含概念示意、类型摘录、历史提案和需要宿主上下文的片段，不能根据比例直接判定文档质量。

建议优先为新增的完整、可直接复用的示例建立稳定提取标识；尤其是屏幕动画、黑幕转场及部分 UI 参考片段，明确标注其编译验证状态。现有[提取脚本](../../../tests/scene/compile_scene_documentation.cmake)依赖代码块顺序，插入片段可能改变抽取对象；后续可考虑显式示例 ID，以降低维护负担。

验收应分别记录：片段编译、运行行为、软件渲染截图、真实窗口和设备体验。不要把示例目标通过写成所有示例都运行过。

## 5. 分领域评价

| 领域 | 当前判断 | 问题或后续重点 |
| --- | --- | --- |
| 中文入口与组织 | 入口、任务索引、参考索引和架构关联较完整 | 范围内本地链接通过；保持报告和现行参考的区别 |
| 构建与游戏接入 | 生成器、工作目录、资源标记、模块与路由前提较清楚 | 测试设备说明需要统一，见 CN-05 |
| 场景与对象 | 所有权、Reuse/Reset/Recreate、安全修改阶段和失败清理说明较完整 | 本次未确认新的内容错误；并非所有运行路径均重新测试 |
| 输入与控制器 | 会话、Pending 结果、设备归属、取消和固定步消费较清楚 | 返回类型摘要见 CN-07 |
| UI | 指南、八个主题参考和类名索引较完整 | 扩展片段验证并保持回调寿命说明，见 R-03 |
| 物理 | 当前 Box2D 模型与生命周期说明总体已整理 | 能力表、Inspector 迁移残留见 CN-03、CN-06 |
| 相机 | 运行时归属、槽位、跟随、演出和坐标空间边界较明确 | 本次核对的多目标策略默认值与头文件一致 |
| 资源与配置格式 | 主要清单、路径、键构造和冲突诊断覆盖较完整 | 当前模块表与样例路径失真，见 CN-02 |
| 动画与特效 | 世界特效和屏幕特效的时钟、创建阶段及清理边界有所区分 | 新片段未全部纳入编译目标，见 R-03 |
| 音频 | 使用指南已更新为浮点轨道增益 | 架构过期与重复维护见 CN-01、R-01 |
| 配置与存档 | 运行状态、持久化、回滚、恢复和未来版本保护说明较细 | 减少完整格式重复；本次未重做全部 I/O 故障注入 |
| 本地化与时间 | 借用结果、缓存失效、时间缩放和暂停区别较清楚 | 本次未确认专属事实错误 |
| 贡献与验证 | 已有示例提取、链接检查及统计自测 | CN-04、CN-05；明确范围与各验证层次 |
| 设计与历史记录 | 网络提案和历史验收已标注状态 | 保留历史日期、基线和未执行验收，不将历史数字冒充本次结果 |

“未确认专属事实错误”只表示本轮交叉核对没有形成可报告的问题，不表示该领域每个 API 已完成逐条运行验收。

## 6. 验证结果与复现方法

### 6.1 已执行的检查

| 检查 | 结果 | 依据与限制 |
| --- | --- | --- |
| 中文链接与锚点 | 85 篇，0 错误 | 本轮显式检查报告新增前的 `docs/zh-CN` 与中文 README |
| 报告及索引交付检查 | 86 篇，0 链接错误 | 新增本报告和更新贡献索引后检查；`git diff --check` 通过 |
| JSON 代码块 | 29/29 语法通过 | 同日同基线的全量盘点；这些 JSON 块全部属于中文范围 |
| C++ 项目头文件路径 | 未发现缺失 | 同日同基线扫描 `engine/...`、`game/...` 的 include |
| 文档示例编译 | 30 个编译单元、42 个块通过 | 同日同基线已执行 MSVC x64 Debug / ImGui ON 构建；本次报告整理未改示例正文 |
| 语言特性检查 | 失败 | 本轮重新执行 `-Check`，仍遇到四个未分类符号 |
| 语言特性脚本自测 | 通过 | 同日同基线已执行；不能抵消 `-Check` 失败 |
| CTest 设备要求 | 已核对源码及注册方式 | 未运行完整测试集，不能据此宣布 155 项测试通过 |

本报告新增后的中文链接检查共 86 篇，比基线增加 1。贡献索引的导航更新也属于本次报告交付的一部分；它不改变上述基线问题的事实依据。

### 6.2 复现命令

在仓库根目录执行：

```powershell
python scripts/check_documentation_links.py docs/zh-CN README.zh-CN.md
pwsh -NoProfile -File scripts/audit_cpp_language_features.ps1 -SelfTest
pwsh -NoProfile -File scripts/audit_cpp_language_features.ps1 -Check
git diff --check
```

在已加载 x64 MSVC 开发环境、完成配置并启用 `BUILD_TESTING` 的环境中验证示例。以下是本次已有 Ninja 构建目录，并非项目固定要求的目录名：

```powershell
cmake --build out/build/x64-Debug --target scene_documentation_examples --parallel 4
ctest --test-dir out/build/x64-Debug --show-only=json-v1
```

单独运行 `ctest` 不会替代文档示例目标的构建；查看注册清单也不等于执行测试。

## 7. 修复工作分组与验收

| 分组 | 内容 | 完成标准 |
| --- | --- | --- |
| 当前事实同步 | CN-01、CN-02、CN-03、CN-06、CN-07 | 文档、头文件、配置和调试面板之间没有对应冲突 |
| 验证流程恢复 | CN-04、CN-05 | 统计检查可复现通过；设备筛选说明与真实用例需求一致 |
| 内容维护收敛 | R-01、R-02、R-03 | 契约有明确主要维护位置，检查范围明确，完整示例的验证方式可追踪 |

建议把修改限定为文档事实同步和必要的检查维护；不要为满足旧说明而恢复已经移除的 API、素材布局或后端统计。若修复需要改动代码行为，应先独立确认预期契约，再据此同步文档与测试。

本次交付只新增本报告并在贡献指南中添加入口，未修正上述问题正文或引擎实现。报告中的问题状态均为“待处理”，不能因报告已保存而认为问题已修复。

## 附录 逐文件检查范围

以下表格对应报告新增前的 85 篇文档。“静态通过”仅表示本地 Markdown 链接、标题锚点与代码围栏未发现错误；“未记录专属问题”不等于逐条 API 运行验收。编译块数来自现有文档示例目标的实际提取结果。

<!-- BASELINE INVENTORY -->
| 文档 | 基线行数 | C++ 块 | 编译块 | 静态检查 | 问题或建议 |
| --- | ---: | ---: | ---: | --- | --- |
| [README.zh-CN.md](../../../README.zh-CN.md) | 119 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/README.md](../README.md) | 21 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/README.md](../architecture/README.md) | 24 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/audio.md](../architecture/subsystems/audio.md) | 378 | 23 | 0 | 静态通过 | CN-01、R-01 |
| [docs/zh-CN/architecture/subsystems/camera-showcase-validation.md](../architecture/subsystems/camera-showcase-validation.md) | 31 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/camera.md](../architecture/subsystems/camera.md) | 219 | 7 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/development-demos.md](../architecture/subsystems/development-demos.md) | 55 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/development-overlay.md](../architecture/subsystems/development-overlay.md) | 43 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/input/README.md](../architecture/subsystems/input/README.md) | 22 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/input/architecture.md](../architecture/subsystems/input/architecture.md) | 112 | 0 | 0 | 静态通过 | CN-07 |
| [docs/zh-CN/architecture/subsystems/input/migration.md](../architecture/subsystems/input/migration.md) | 33 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/input/testing-and-debugging.md](../architecture/subsystems/input/testing-and-debugging.md) | 86 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/01-current-state-audit.md](../architecture/subsystems/physics/01-current-state-audit.md) | 27 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/02-target-architecture.md](../architecture/subsystems/physics/02-target-architecture.md) | 19 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/03-class-responsibilities.md](../architecture/subsystems/physics/03-class-responsibilities.md) | 18 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/04-function-responsibilities.md](../architecture/subsystems/physics/04-function-responsibilities.md) | 23 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/05-collision-algorithms.md](../architecture/subsystems/physics/05-collision-algorithms.md) | 17 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/06-tile-map-collision.md](../architecture/subsystems/physics/06-tile-map-collision.md) | 13 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/07-gameplay-collision-runtime.md](../architecture/subsystems/physics/07-gameplay-collision-runtime.md) | 15 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/08-implementation-roadmap-and-tests.md](../architecture/subsystems/physics/08-implementation-roadmap-and-tests.md) | 49 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/physics/README.md](../architecture/subsystems/physics/README.md) | 20 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/resources/README.md](../architecture/subsystems/resources/README.md) | 37 | 0 | 0 | 静态通过 | CN-02 |
| [docs/zh-CN/architecture/subsystems/runtime-config.md](../architecture/subsystems/runtime-config.md) | 92 | 0 | 0 | 静态通过 | R-01 |
| [docs/zh-CN/architecture/subsystems/save.md](../architecture/subsystems/save.md) | 108 | 2 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/scene-lifecycle.md](../architecture/subsystems/scene-lifecycle.md) | 50 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/showcase-validation.md](../architecture/subsystems/showcase-validation.md) | 29 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/architecture/subsystems/ui/README.md](../architecture/subsystems/ui/README.md) | 232 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/contributing/README.md](README.md) | 15 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/contributing/coding-standard.md](coding-standard.md) | 80 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/contributing/cpp-language-features.md](cpp-language-features.md) | 183 | 0 | 0 | 静态通过 | CN-04 |
| [docs/zh-CN/contributing/documentation.md](documentation.md) | 135 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/contributing/testing.md](testing.md) | 63 | 0 | 0 | 静态通过 | CN-05、R-02 |
| [docs/zh-CN/design/README.md](../design/README.md) | 12 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/design/player-host-networking-and-input-design.md](../design/player-host-networking-and-input-design.md) | 341 | 1 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/design/roadmap.md](../design/roadmap.md) | 26 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/README.md](../reference/README.md) | 12 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/input/README.md](../reference/input/README.md) | 6 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/input/action-mapping.md](../reference/input/action-mapping.md) | 49 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/input/controllers.md](../reference/input/controllers.md) | 46 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/physics/README.md](../reference/physics/README.md) | 127 | 0 | 0 | 静态通过 | CN-03、R-01 |
| [docs/zh-CN/reference/physics/units-and-state.md](../reference/physics/units-and-state.md) | 52 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/resources/README.md](../reference/resources/README.md) | 62 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/resources/animation-and-effects.md](../reference/resources/animation-and-effects.md) | 167 | 0 | 0 | 静态通过 | CN-02 |
| [docs/zh-CN/reference/resources/bootstrap-and-registry.md](../reference/resources/bootstrap-and-registry.md) | 89 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/resources/core-resource-manifests.md](../reference/resources/core-resource-manifests.md) | 104 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/resources/entity-content.md](../reference/resources/entity-content.md) | 118 | 0 | 0 | 静态通过 | CN-02 |
| [docs/zh-CN/reference/ui/README.md](../reference/ui/README.md) | 61 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/ui/controls-and-selection.md](../reference/ui/controls-and-selection.md) | 225 | 2 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/ui/elements-and-layout.md](../reference/ui/elements-and-layout.md) | 163 | 4 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/ui/input-and-focus.md](../reference/ui/input-and-focus.md) | 64 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/ui/media-and-animation.md](../reference/ui/media-and-animation.md) | 65 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/ui/settings-panel.md](../reference/ui/settings-panel.md) | 70 | 2 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/ui/style-and-theme.md](../reference/ui/style-and-theme.md) | 74 | 1 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/ui/text-and-values.md](../reference/ui/text-and-values.md) | 76 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/reference/ui/windows-and-popups.md](../reference/ui/windows-and-popups.md) | 116 | 1 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/README.md](../user-guide/README.md) | 154 | 1 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/build-and-run.md](../user-guide/build-and-run.md) | 115 | 0 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/builtin-scenes/application-failure.md](../user-guide/builtin-scenes/application-failure.md) | 79 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/builtin-scenes/settings.md](../user-guide/builtin-scenes/settings.md) | 85 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/builtin-scenes/startup-loading.md](../user-guide/builtin-scenes/startup-loading.md) | 67 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/content/configuration.md](../user-guide/content/configuration.md) | 142 | 2 | 2 | 静态通过 | R-01 |
| [docs/zh-CN/user-guide/content/resources.md](../user-guide/content/resources.md) | 122 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/content/save.md](../user-guide/content/save.md) | 161 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/core_concepts.md](../user-guide/core_concepts.md) | 531 | 19 | 0 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/game-initialization.md](../user-guide/game-initialization.md) | 246 | 4 | 3 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/gameplay/collision.md](../user-guide/gameplay/collision.md) | 133 | 3 | 3 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/gameplay/control.md](../user-guide/gameplay/control.md) | 177 | 4 | 2 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/gameplay/damage.md](../user-guide/gameplay/damage.md) | 368 | 2 | 2 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/gameplay/scene.md](../user-guide/gameplay/scene.md) | 92 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/gameplay/world-ui.md](../user-guide/gameplay/world-ui.md) | 69 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/scene/game-objects.md](../user-guide/scene/game-objects.md) | 84 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/scene/scene.md](../user-guide/scene/scene.md) | 161 | 2 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/systems/animation-and-effects.md](../user-guide/systems/animation-and-effects.md) | 289 | 5 | 1 | 静态通过 | R-03 |
| [docs/zh-CN/user-guide/systems/audio.md](../user-guide/systems/audio.md) | 283 | 15 | 1 | 静态通过 | R-01 |
| [docs/zh-CN/user-guide/systems/camera.md](../user-guide/systems/camera.md) | 121 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/systems/input.md](../user-guide/systems/input.md) | 129 | 2 | 2 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/systems/localization.md](../user-guide/systems/localization.md) | 90 | 2 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/systems/object-query.md](../user-guide/systems/object-query.md) | 57 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/systems/physics.md](../user-guide/systems/physics.md) | 228 | 4 | 4 | 静态通过 | R-01 |
| [docs/zh-CN/user-guide/systems/tilemap-collision.md](../user-guide/systems/tilemap-collision.md) | 52 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/systems/time-and-timers.md](../user-guide/systems/time-and-timers.md) | 110 | 2 | 2 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/tools/debug-draw.md](../user-guide/tools/debug-draw.md) | 49 | 1 | 1 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/tools/development-panels.md](../user-guide/tools/development-panels.md) | 136 | 1 | 1 | 静态通过 | CN-06 |
| [docs/zh-CN/user-guide/tools/utilities.md](../user-guide/tools/utilities.md) | 119 | 5 | 2 | 静态通过 | 未记录专属问题 |
| [docs/zh-CN/user-guide/ui/README.md](../user-guide/ui/README.md) | 158 | 1 | 1 | 静态通过 | 未记录专属问题 |
