# Generic Save Service

This document explains in-memory data, file replacement, and diagnostics for save-module maintainers and game-format designers. Calling examples handle each result before dependent operations.

`engine/save/` supplies typed `persistence` independent of specific Gameplay data. The engine validates names, manages memory documents, encodes JSON, writes reliably, and recovers copies. Gameplay defines keys, business validation, migration, and save timing.

## Basic Usage

`Application` initializes `SaveService` with `PathManager::saves()`. Callers supply logical names; the engine appends `.json` in the fixed directory:

```cpp
#include "engine/save/save_service.h"
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

// Application already initialized the service; this function creates a save.
// Callers handle existing names and other failures; failed creation does not authorize overwrite.
std::expected<void, elysia::save::SaveFailure> save_progress()
{
    auto* saves = ELYSIA_SAVE;
    auto created = saves->create("slot_01");
    if (!created) return std::unexpected(created.error());
    auto level = saves->set("slot_01", "player.level", std::int64_t{12});
    if (!level) return std::unexpected(level.error());
    auto stages = saves->set("slot_01", "progress.unlocked_stages",
        std::vector<std::string>{"forest_01", "forest_02"});
    if (!stages) return std::unexpected(stages.error());
    return saves->commit("slot_01");
}
```

The function demonstrates complete result propagation, not a standalone program. Check the final result and log diagnostics. Intermediate failure may leave an open in-memory save; choose retry or explicit discard according to business rules. Explicitly open existing saves before reading:

```cpp
auto opened = ELYSIA_SAVE->open("slot_01");
if (!opened) return;

auto level = ELYSIA_SAVE->get<std::int64_t>(
    "slot_01",
    "player.level");
```

The service caches multiple open documents. Changes update dirty state and revision; assigning the same value creates no revision. `close()` refuses dirty data by default. First `commit()` or explicitly select `SaveClosePolicy::DiscardChanges`.

## Data Types

`SaveData` accepts only these exact `types`, with no implicit numeric/string conversion:

- `bool`, `std::int64_t`, `double`, `std::string`
- `std::vector<bool>`, `std::vector<std::int64_t>`, `std::vector<double>`, `std::vector<std::string>`

All doubles must be finite. Gameplay codecs flatten complex objects into stable keys; runtime pointers, scenes, and entity instances cannot be stored directly.

## File Format

JSON format version 1 uses a separate type table, preserving the exact type of empty arrays after restart:

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

The root permits only `format_version`, `types`, and `values`; both data-key sets must match exactly. Store the game's data version as an ordinary value such as `gameplay.schema_version`.

## Names and Recovery

Save names contain 1–64 ASCII letters, digits, `_`, or `-`, without an extension or path. `slot_01` corresponds to:

```text
player_data/saves/slot_01.json
player_data/saves/slot_01.json.tmp
player_data/saves/slot_01.json.bak
```

Commit writes and validates a temporary file, rotates the backup, then replaces the main file. Opening a corrupt main archives it as `.corrupt` and tries `.tmp`, then `.bak`. Without a valid copy, return an `error` rather than create an empty save. Future format versions remain untouched and directly return `UnsupportedFormatVersion`, without fallback to older backups.

The first version provides synchronous I/O only. Autosave triggers, asynchronous queues, and project-specific fixed-slot enumeration are future Gameplay/engine orchestration work.

## Diagnostics and Commit Boundaries

`SaveFailure` retains `error`, `save_name`, and key. Messages, origins, and extra entries live in `diagnostic`, without a separate `message`. `SaveOpenResult::warning` is `optional<SaveFailure>`; successful recovery may still report corruption, archival, and recovery-copy diagnostics.

Disk failures may carry `persistence`: stage, main/temp/backup paths and known existence states, plus recovery results `NotRequired`, `Succeeded`, `Failed`, or `Unknown`. `Unknown` is not absence; existence is not validity. Preserve the original failure and append recovery/state-query failures.

Prepare serialization first. If the main is missing, validate/promote an existing temporary copy before writing a new temporary file. Check write, flush, close, and content validation before rotating and replacing. Retain an existing backup when both main and valid temp are missing. Restore the old main after replacement failure; retries must not delete the only recovery copy. `Failed` promotion preserves the temporary file.

Access, permission, and read failures return errors and do not archive files as corrupt. Archive and recover only after establishing invalid content. `Failed` corrupt-main archival stops loading and preserves the original with full context.

Expected I/O/data failures return `expected`; allocation and internal-contract exceptions propagate. If an exception occurs after moving main to backup, attempt protected recovery before propagating the original exception. Cleanup logs recovery failures. The Store does not duplicate logging for ordinary returned failures; the consuming boundary logs and chooses retry, prompt, or exit.

Commit failure retains dirty state and revision. File replacement does not guarantee power-loss durability, cross-process exclusion, or concurrent writes.

### Protect Temporary Copies before Saving

Inspect main first. If missing with an existing `.tmp`, reuse load validation: promote a valid copy before new writes; overwrite only confirmed corruption; stop on access failure or future version. `Failed` promotion preserves content. Valid `.tmp` takes precedence over old `.bak`; normal rotation then backs up the recovered main. Promotion alone does not mean new data was saved.

Save the original recovery result before constructing diagnostics. If later state queries or `diagnostic` allocation throw, retain the primary failure and collected recovery errors in logs. Finish necessary recovery, then propagate the original exception; never repeat successful recovery. Logging may degrade under persistent allocation failure, and cleanup must not throw a second exception.
