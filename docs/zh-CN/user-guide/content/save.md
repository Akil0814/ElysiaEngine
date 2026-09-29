# 存档读写与恢复

应用负责在 `player_data/saves/` 初始化 `ELYSIA_SAVE`。游戏负责字段、业务校验、版本迁移和保存时机，不自行初始化或关闭服务。

## 写入已有存档

以下函数放在游戏存档代码中，在关卡结算等明确保存点调用。前提是槽位已经创建并提交过；打开失败不会覆盖或创建空槽位。

```cpp
#include "engine/save/save_service.h"
#include "engine/tools/logger.h"
#include <cstdint>

bool save_level() {
    const auto log_failure = [](const elysia::save::SaveFailure& failure,
        elysia::tools::LogLevel level = elysia::tools::LogLevel::Error) {
        elysia::tools::Logger::instance()->log_stream(level, "save",
            [&](std::ostream& output) {
                output << elysia::core::format_failure_diagnostic(
                    failure.diagnostic, "SAVE", "save");
            }, failure.diagnostic.origin);
    };
    auto* saves = ELYSIA_SAVE;
    auto opened = saves->open("slot_01");
    if (!opened) {
        log_failure(opened.error());
        return false;
    }
    if (opened->warning)
        log_failure(*opened->warning, elysia::tools::LogLevel::Warn);
    auto changed = saves->set("slot_01", "player.level", std::int64_t{12});
    if (!changed) {
        log_failure(changed.error());
        (void)saves->close("slot_01", elysia::save::SaveClosePolicy::DiscardChanges);
        return false;
    }
    auto committed = saves->commit("slot_01");
    if (!committed) {
        log_failure(committed.error());
        // 本例选择放弃内存编辑；需要重试的业务应保留打开文档并另行管理。
        (void)saves->close("slot_01", elysia::save::SaveClosePolicy::DiscardChanges);
        return false;
    }
    auto closed = saves->close("slot_01");
    if (!closed) log_failure(closed.error());
    return closed.has_value();
}
```

新游戏先调用 `create(name)`，成功后 set 初始字段再 commit；默认 FailIfExists 防止覆盖既有存档。只有玩家明确选择覆盖时才用 OverwriteExisting。失败分支仍按上述方式管理打开文档，不把 open 的损坏或未来版本错误当成“新游戏”。

读取流程是 open → `get<T>(name, key)` → 校验业务数值 → close。get 返回 expected 值副本，必须检查 KeyNotFound、TypeMismatch 等错误；例如 `get<std::int64_t>` 不能改成普通 int。不要对未打开槽位调用字段读写。

## 常用操作与状态

| 操作 | 使用规则 |
| --- | --- |
| `contains` / `keys` / `erase` | 对已打开文档检查、枚举或删除字段；均检查 expected |
| `snapshot` / `replace` | 复制或替换内存数据，适合先校验完整迁移结果再提交 |
| `is_dirty` / `revision` | 查询内存修改；相同值不产生新 revision |
| `commit` | 把当前内存数据写入磁盘；set 本身不持久化 |
| `close` | 默认拒绝丢弃脏数据；显式 DiscardChanges 才放弃编辑 |
| `exists` / `list_save_names` | 查询磁盘存档，不等同于打开或验证全部业务数据 |
| `remove` | 先关闭该存档，再删除并检查返回错误；在业务确认删除后调用 |

服务可同时缓存多个文档。同一槽位由一个明确负责人管理，避免一个组件 close 掉另一个组件正在编辑的文档。退出场景不会自动保存；应在退出前明确 commit 或丢弃，而不是依赖应用析构。

字段 key 必须非空；示例中的点是游戏命名约定，存档按完整字符串保存字段，并不自动建立嵌套对象。不要把配置服务的递归展开规则套用到存档上。
## 数据类型

`SaveData` 只接受以下精确类型，不执行数值或字符串隐式转换：

- `bool`、`std::int64_t`、`double`、`std::string`
- `std::vector<bool>`、`std::vector<std::int64_t>`、`std::vector<double>`、`std::vector<std::string>`

所有 `double` 必须有限。复杂 Gameplay 对象应由 Gameplay codec 展开为稳定 key；运行时指针、Scene 或 Entity 实例不能直接写入。

## 文件格式

JSON format version 1 使用独立类型表，因此空数组也能在重启后保持精确类型：

```json
{
  "format_version": 1,
  "types": {
    "player.level": "int64",
    "progress.unlocked_stages": "string_array"
  },
  "values": {
    "player.level": 12,
    "progress.unlocked_stages": []
  }
}
```

根对象只允许 `format_version`、`types`、`values`，两个 key 集合必须完全一致。Gameplay schema version 应作为普通值保存，例如 `gameplay.schema_version`。

## 名称与恢复

存档名必须由 1–64 个 ASCII 字母、数字、`_` 或 `-` 组成，不能携带扩展名或路径。`slot_01` 对应：

```text
player_data/saves/slot_01.json
player_data/saves/slot_01.json.tmp
player_data/saves/slot_01.json.bak
```

提交时先写入并验证临时文件，再轮换备份并替换主文件。打开损坏主文件时会将其归档为 `.corrupt`，随后依次尝试 `.tmp` 和 `.bak`。没有有效副本时返回错误，不创建空存档。未来 format version 会原样保留并直接返回 `UnsupportedFormatVersion`，不会降级到旧备份。

存档读写是同步 IO。自动保存时机由游戏决定；`list_save_names()` 可枚举已有逻辑存档名，具体槽位的 UI 和业务规则由游戏实现。

## 兼容性与常见误用

引擎的 format_version 是容器格式，不是游戏数据版本。游戏保存自己的 schema version，先读版本再校验、迁移字段；遇到未知未来版本应保留文件并拒绝按旧规则覆盖。恢复成功仍检查 recovered 与 warning，必要时告知玩家使用了备用副本。

不要保存原始指针、场景实例或临时运行句柄；保存稳定业务标识和纯数据，在载入后重建对象关联。不要每帧 commit 同步 IO，也不要把“内存修改成功”当作“磁盘保存成功”。

- [服务接口](../../../../engine/save/save_service.h)、[错误类型](../../../../engine/save/save_types.h)
- [场景生命周期](../scene/scene.md)、[返回使用指南](../README.md)

## 消费失败与恢复警告

所有返回结果都必须检查。可预期的文件访问、写入、校验和恢复失败通过 `expected` 返回，不因保存失败自动退出应用。调用方依据 `error` 和可选的 `persistence` 决定提示或重试；保存失败后内存修改仍为 dirty，不要显示“已保存”。

`SaveFailure::diagnostic` 是唯一报告来源，可通过 `format_failure_diagnostic` 写入日志或展示。`SaveOpenResult::warning` 为可选的 `SaveFailure`，恢复成功后仍应检查。旧的 `SaveFailure::message` 和字符串 warning 已删除。

恢复结果与文件存在状态分别记录。恢复失败或状态未知时保留现有副本，重新加载并检查结果后再决定重试；不能因备份存在就断言数据有效，也不能把状态查询失败当作文件缺失。

### 保存前的临时副本保护

保存前先检查主文件。主文件缺失且已有 `.tmp` 时，复用加载校验：有效副本先提升为主文件，再开始新写入；确认损坏才允许重写，访问失败或未来版本则停止。提升失败保留副本原内容。有效 `.tmp` 优先于旧 `.bak`，后续正常轮换以恢复后的主文件为备份；提升本身不代表新内容已经保存成功。

恢复操作的原始结果在构造诊断前保存。恢复后的状态查询或诊断分配再次异常时，日志仍保留主失败和此前恢复错误；必要恢复完成后传播原异常，成功恢复不重复执行。持续内存不足时日志可降级，清理边界不得抛出二次异常。
