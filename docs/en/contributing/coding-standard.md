# Elysia Engine Layer Coding Standard

This document describes naming, lifecycle interfaces, and failure-handling conventions for developers modifying engine-layer C++. These conventions make responsibilities, ownership, and error boundaries clear to callers. Before changing an interface, check its source and tests; do not change behavior merely for stylistic uniformity.

## Code Naming Conventions

- Classes, structures, and enums use `PascalCase`.
- Enumerators use `PascalCase`.
- Private and protected members use `_name_name`.
- Public data members use `name_name`.
- Local variables and function parameters use `name_name`.
- Functions and member functions use `name_name`.
- Filenames and namespaces use `name_name`.
- Ordinary `const` variables follow the conventions of their scope.
- Named compile-time constants use `kPascalCase`.
- Macros use `ELYSIA_UPPER_SNAKE_CASE`.

### Class and Type Names

- Use nouns or noun phrases that accurately describe a class's responsibility.
- Do not use Hungarian type prefixes such as `C`, `I`, or `S`.
- `Abstract` classes and interfaces do not require `Abstract`, `Base`, or `I` prefixes.
- Use `Base` only when it clarifies the type's role.
- Treat acronyms as ordinary words, as in `UiWindow`.
- Template type parameters use `T` or `TName`.
- Type aliases use `PascalCase`.

#### Manager

- A `Manager` is internal to the engine and manages object or resource lifetimes, state, and coordination between related modules.
- Managers generally are not exposed directly to the game layer. Not every `Manager` is a singleton.

#### Service

- A `Service` exposes engine capabilities to the game layer through explicit interfaces, hiding internal management and implementation details.
- Not every publicly exposed class needs a `Service` suffix. Name data types and object models according to their responsibilities.

## Lifecycle Naming

- `initialize()` / `shutdown()`: establish and release a module's usable state.
- `start()` / `update()` / `reset()`: start, advance, and reset an ongoing or asynchronous flow.
- `load()`: read content from files or other external sources.
- `configure()`: update configuration without managing the entire module lifecycle.
- `bind()`: inject or connect runtime dependencies.
- `is_initialized()`: query whether initialization has completed.

Do not rename a flow-oriented `start()` or a data-reading `load()` to `initialize()` solely for uniformity. Design initialization and shutdown as a pair. If initialization state is public, use `is_initialized()` consistently.

## Using auto

- Prefer explicit types by default, especially in public engine APIs and core logic.
- Do not hide type information with `auto` when the type is not immediately apparent.
- `auto` is appropriate for iterators, generic code, lambdas, and smart-pointer factories.
- Do not conceal significant pointer, reference, or ownership semantics.
- Do not use `auto` solely to shorten the code.

## Code Completeness

- Source code must compile at every commit.
- Mark unfinished implementations with `TODO`.
- Unimplemented features must not pretend to return a normal successful result.
- Ensure relevant tests pass when modifying existing functionality.

## Error Handling

- Expected failures in files, configuration, resources, and recoverable operations use `[[nodiscard]] std::expected`; callers must consume results. Missing-query results, optional effects, and playback rejection may retain explicit empty values or status results.
- Persistence I/O, validation, and recovery failures return `expected`; ordinary save failures are not exceptions. Store messages and origins only in `FailureDiagnostic`. Domain failures retain error enums and business identifiers without duplicate message fields.
- Express disk recovery and runtime rollback separately. Preserve the original failure and every subsequent recovery error. Unknown file state is not absence, and existence does not establish validity. Access failures must not cause corruption archival or replacement with defaults.
- Prepare serialization and in-memory commit state before replacing files. Check temporary-file writing, flush, close, and validation. State publication after a successful write must neither allocate nor throw. Protect an existing valid temporary copy when the main file is missing; retain existing backups when no valid temporary copy exists.
- Catch specific file or external-data exceptions. Do not use `catch(std::exception)` to wrap allocation failures or internal errors as I/O or parsing failures. After file mutation, perform protected necessary recovery before propagating unexpected exceptions. That cleanup boundary logs recovery failures; the boundary consuming an ordinary failure result logs that result.
- Propagate `FailureDiagnostic` across layers, retaining the original origin, resource key, declaration location, and recovery errors. Do not extract only a message and reconstruct the diagnostic. Ordinary exceptions record the catch location; do not claim it is the original throw location.
- Use the typed scene key and boundary fields of `SceneBoundaryFailure`. Generate logs, routes, and reports consistently through `to_failure_diagnostic()`; do not pre-copy the primary scene entry or scan text to reconstruct context.
- `SceneManager` catches scene logic exceptions, performs cleanup, and enters `ApplicationFailureScene`. Failed recovery reports a fault without recursive recovery. Fault observers are isolated, and notification failures must also be logged.
- Scene cleanup first collects exceptions and continues reclamation, then constructs diagnostics and decides propagation. If any cleanup stage encounters a rendering-backend failure, exit with the first backend failure while preserving the original scene exception and later cleanup failures. Runtime helpers must not merely log and swallow backend exceptions. The final shutdown boundary remains non-throwing.
- Rendering commands share an internal execution implementation. Both single-command and batch public interfaces convert typed backend exceptions to `expected` only at their outermost boundary; do not repeatedly convert exceptions and results inside forwarding calls or batch loops.
- SDL render-state reading, drawing, restoration, clearing, or presentation failures lead to application fault exit. Restore modified state, then stop executing commands. Do not attempt to display a failure scene using the same renderer.
- Worker threads require an entry exception boundary that hands the first exception and task context back to the main thread. Workers do not perform global cleanup. The main thread stops dispatch, joins all workers, and cleans partial results.
- Restore actual state after a failed multi-step modification, rather than relying solely on configuration-value changes to trigger restoration. Continue other restoration steps and preserve both the original and restoration failures.
- `Logger` records information and does not control exit. Log complete diagnostics at the handling boundary. `TerminationManager` retains the first termination request, copies bounded text, and marks truncation; `Application` performs coordinated shutdown.
- Mark a path `noexcept` only when it is truly guaranteed not to throw. Rendering helpers that allocate or must propagate exceptions cannot rely on `noexcept` to suppress failures. Shutdown boundaries catch failures, continue cleanup, and return shutdown results.
- When removing old contracts, migrate engine, game, tests, and documentation together. Do not retain unauthorized compatibility wrappers.

Persistence cleanup must save the original recovery result before constructing or appending diagnostics. Later queries or diagnostic allocation failures must not discard already collected failures, and successful recovery must not run twice. Ordinary settings failures show a short localized UI message; the boundary consuming the result logs the full diagnostic. JSON read failures are file-access failures, not content corruption.
