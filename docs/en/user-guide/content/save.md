# Save Data, Writes, and Recovery

This guide converts game data to typed saves and handles `commit`/recovery results. The service writes files and recovers copies; games own semantics, business migrations, and save timing.

The application initializes `ELYSIA_SAVE` at `player_data/saves/`. Games own fields, validation, migration, and timing, without initializing or closing the service.

## Writing an Existing Save

Place this in save code and call at explicit save points such as level results. The slot must already have been created/committed, and no other flow may hold its open document. This function owns the entire open-to-close use period. Failed open neither overwrites nor creates an empty slot.

```cpp
#include "engine/save/save_service.h"
#include "engine/tools/logger.h"

#include <cstdint>
#include <expected>

bool save_level()
{
    const auto log_failure = [](const elysia::save::SaveFailure& failure,
        elysia::tools::LogLevel level = elysia::tools::LogLevel::Error)
    {
        elysia::tools::Logger::instance()->log_stream(level, "save",
            [&](std::ostream& output)
            {
                output << elysia::core::format_failure_diagnostic(
                    failure.diagnostic, "SAVE", "save");
            }, failure.diagnostic.origin);
    };

    elysia::save::SaveService* saves = ELYSIA_SAVE;
    // Discard this example's memory edits after failure; preserve close failures too.
    const auto discard_changes = [&]
    {
        const std::expected<void, elysia::save::SaveFailure> closed =
            saves->close("slot_01", elysia::save::SaveClosePolicy::DiscardChanges);
        if (!closed)
            log_failure(closed.error());
    };

    const std::expected<elysia::save::SaveOpenResult, elysia::save::SaveFailure> opened =
        saves->open("slot_01");
    if (!opened)
    {
        log_failure(opened.error());
        return false;
    }
    if (opened->warning)
        log_failure(*opened->warning, elysia::tools::LogLevel::Warn);

    // set edits memory only; field types must exactly match the save's int64 contract.
    const std::expected<void, elysia::save::SaveFailure> changed =
        saves->set("slot_01", "player.level", std::int64_t{12});
    if (!changed)
    {
        log_failure(changed.error());
        discard_changes();
        return false;
    }
    // Report save success only after commit succeeds.
    const std::expected<void, elysia::save::SaveFailure> committed =
        saves->commit("slot_01");
    if (!committed)
    {
        log_failure(committed.error());
        // This discards edits; retrying flows should retain/manage the open document separately.
        discard_changes();
        return false;
    }
    const std::expected<void, elysia::save::SaveFailure> closed = saves->close("slot_01");
    if (!closed)
        log_failure(closed.error());
    return closed.has_value();
}
```

New games call `create(name)`, set initial fields, then `commit`. Default FailIfExists prevents overwriting. Use OverwriteExisting only after explicit player choice. Manage open documents on failure as above; corrupt/future-version open errors are not new games.

Reading: open → `get<T>(name, key)` → business validation → close. get returns `expected` value copies; check KeyNotFound, TypeMismatch, etc. For example, `get<std::int64_t>` cannot become ordinary int. Do not access fields in unopened slots.

## Common Operations and State

| Operation | Rule |
| --- | --- |
| `contains` / `keys` / `erase` | Check/enumerate/`remove` open-document fields; check `expected` |
| `snapshot` / `replace` | Copy/`replace` memory data, useful for validating whole migrations before `commit` |
| `is_dirty` / `revision` | Query memory changes; identical `values` create no `revision` |
| `commit` | Persist current memory; set alone does not save |
| close | Refuse dirty data by default; explicit DiscardChanges abandons edits |
| `exists` / `list_save_names` | Query disk saves without opening/validating all business data |
| `remove` | Close first, then delete/check errors after business confirmation |

Multiple documents can be cached. Give each slot one clear owner so components do not close each other's edits. Scene exit does not automatically save; `commit`/discard explicitly before exit, not in application destruction.

Field `keys` must be nonempty. Dots are game naming conventions; saves store the full string without creating nested objects. Configuration's recursive expansion does not apply.
## Data Types

`SaveData` accepts only these exact `types`, without implicit numeric/string conversion:

- `bool`, `std::int64_t`, `double`, `std::string`
- `std::vector<bool>`, `std::vector<std::int64_t>`, `std::vector<double>`, `std::vector<std::string>`

Every `double` must be finite. Game encoders/decoders convert complex objects into these `values`; runtime pointers, scenes, and entity instances cannot be written directly.

## File Format

JSON format version 1 uses a separate type table so empty arrays retain exact `types` after restart:

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

Only `format_version`, `types`, `values` are allowed at the root; data-key sets must match exactly. Store game schema versions as ordinary `values`, such as `gameplay.schema_version`.

## Names and Recovery

Names are 1–64 ASCII letters/digits/`_`/`-`, without extension/path. `slot_01` maps to:

```text
player_data/saves/slot_01.json
player_data/saves/slot_01.json.tmp
player_data/saves/slot_01.json.bak
```

Commit writes/verifies temporary data before rotating backup and replacing main. Corrupt main files archive as `.corrupt`, then recovery tries `.tmp` and `.bak`. No valid copy returns failure without creating empty data. Future format versions remain untouched and immediately return `UnsupportedFormatVersion`, without fallback to older backups.

I/O is synchronous. Games choose autosave timing. `list_save_names()` enumerates logical names; games implement slot UI/rules.

## Compatibility and Common Misuse

Engine `format_version` describes the container, not game data. Save your own schema version, read it before validating/migrating fields, and preserve/reject unknown future data rather than overwrite. Successful recovery still requires checking recovered/warning and, when needed, notifying the player about backup use.

Save stable business IDs and plain data, not raw pointers, scenes, or temporary handles; rebuild associations on load. Do not `commit` synchronous I/O every frame or equate memory mutation with disk `persistence`.

## Handling Failures and Recovery Warnings

Check all results. Expected file access/write/validation/recovery errors return through `expected` without automatically exiting. Use `error` and optional `persistence` to choose notification/retry. After failed save, memory remains dirty; do not display saved.

`SaveFailure::diagnostic` retains message, source, and extra failures; log with `format_failure_diagnostic()`. `SaveOpenResult::warning` is optional `SaveFailure` and must be checked after recovery. Player messages should explain outcome/next step, leaving paths/technical details to logs.

Recovery results and file existence are recorded separately. Failed/unknown recovery preserves copies; reload/check before retrying. Backup existence does not prove validity, and failed status queries do not mean missing files.

If main is missing but valid `.tmp` `exists` before saving, the service recovers it before writing new data. Access failures/future versions prevent overwriting; do not delete `.tmp` as a fix. Successful recovery does not mean this new `commit` succeeded; use final `commit()` results.

These steps guarantee neither power-loss durability, cross-process exclusion, nor concurrent writes. Unexpected exceptions such as allocation failures can propagate. Preserve diagnostics and use existing boundaries rather than escalating every ordinary save `error` to application exit.

## References

- [Service API](../../../../engine/save/save_service.h), [error types](../../../../engine/save/save_types.h)
- [Replacement/recovery implementation](../../architecture/subsystems/save.md), [scene lifetime](../scene/scene.md)
- [Back to user guide](../README.md)
