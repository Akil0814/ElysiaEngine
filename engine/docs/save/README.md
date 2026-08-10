# 通用存档服务

`engine/save/` 提供与具体 Gameplay 数据无关的类型化存档能力。引擎负责名称校验、内存文档、JSON 编码、可靠写入和恢复；Gameplay 负责 key、业务校验、数据迁移以及保存时机。

## 基本使用

`Application` 会使用 `PathManager::saves()` 初始化 `SaveService`。调用方只提供逻辑存档名，引擎自动在固定目录补充 `.json`：

```cpp
ELYSIA_SAVE->create("slot_01");
ELYSIA_SAVE->set("slot_01","player.level",std::int64_t{12});
ELYSIA_SAVE->set(
    "slot_01",
    "progress.unlocked_stages",
    std::vector<std::string>{"forest_01","forest_02"});
ELYSIA_SAVE->commit("slot_01");
```

读取已有存档前必须显式打开：

```cpp
auto opened = ELYSIA_SAVE->open("slot_01");
if (!opened) return;

auto level = ELYSIA_SAVE->get<std::int64_t>(
    "slot_01",
    "player.level");
```

服务可以同时缓存多个打开的存档。修改会更新对应文档的 dirty 状态和 revision；设置相同值不会产生新 revision。`close()` 默认拒绝丢弃脏文档，必须先 `commit()`，或者显式使用 `SaveClosePolicy::DiscardChanges`。

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

第一版只提供同步 IO。自动存档触发、异步队列和 Moonline 的固定槽位枚举属于后续 Gameplay/引擎编排工作。
