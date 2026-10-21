# Testing and Documentation Example Validation

This document explains how developers modifying the engine and tests can validate current changes. CTest manages automated tests under `tests/`; complete `scene` examples in documentation have a separate compilation target.

## 1. Configure and Build

Configure the project using the [build guide](../user-guide/build-and-run.md), keeping `BUILD_TESTING=ON`. The following uses `out/build/sdl3-Debug`. If an existing directory uses a different generator, retain its configuration rather than switching generators in place.

```powershell
cmake --build out/build/sdl3-Debug --config Debug --parallel
ctest --test-dir out/build/sdl3-Debug -C Debug --output-on-failure
```

Test targets link `engine_lib` through `add_elysia_test`; some integration tests also link `game_lib`. Windows builds copy SDL runtime DLLs for these targets. `--output-on-failure` shows output from failed tests.

## 2. Select Tests by Label

Ordinary test labels are `application`, `audio`, `bootstrap`, `builtin`, `camera`, `config`, `core`, `effects`, `elysia`, `gameplay_ui`, `gpu`, `input`, `loading`, `localization`, `object_query`, `persistence`, `physics`, `save`, `scene`, `tools`, `typography`, and `ui`.

Physics scenarios and optional stress cases also use `physics_scenario` and `physics_stress`.

```powershell
# List tests and labels registered by the current configuration
ctest --test-dir out/build/sdl3-Debug -C Debug -N
ctest --test-dir out/build/sdl3-Debug -C Debug --print-labels

# -L uses regular expressions; boundaries match only the specified label
ctest --test-dir out/build/sdl3-Debug -C Debug -L '^physics$' --output-on-failure
ctest --test-dir out/build/sdl3-Debug -C Debug -L '^ui$' --output-on-failure
```

Device requirements are maintained in the [build guide](../user-guide/build-and-run.md#running-tests). `gpu` tests require a real graphics device and usable driver; dummy video cannot replace them. `-LE '^gpu$'` excludes only tests carrying that label and does not validate every environment requirement of remaining tests. Application lifecycle branches needing GPU carry both `application` and `gpu`; the two `manager_*` branches carry only `application`. In headless environments, select tests according to actual graphics, `audio`, and filesystem requirements and record exclusions. Software-rendering tests do not constitute GPU or window-interaction acceptance.

## 3. Compile Documentation Examples

```powershell
cmake --build out/build/sdl3-Debug --config Debug --target scene_documentation_examples scene_documentation_examples_en
```

The [Markdown extraction script](../../../tests/scene/compile_scene_documentation.cmake) extracts designated C++ blocks directly from documentation to check `scene`, object, service, and UI examples against public interfaces. The script defines the files and block counts covered; not every conceptual or context-dependent snippet is included. This is a build target, not a CTest test. Running `ctest` alone does not validate examples.

When modifying extracted documents, retain the configured code-block order and example types. If extraction must change, update the script too. Do not insert new local snippets before extracted blocks. Conceptual snippets must state context and omissions and must not be labeled standalone programs.

## 4. Optional Physics Validation

`ELYSIA_ENABLE_FULL_PHYSICS_STRESS_TESTS=ON` registers additional medium/high stress tests. These carry `physics_stress`, run serially, and allow longer timeouts; everyday default configurations do not register them.

`ELYSIA_BUILD_PHYSICS_BENCHMARK=ON` builds the independent `physics_benchmark`. Use Release for measurements and record toolchain, environment, and scope. Historical results are in [Physics Validation and Performance](../architecture/subsystems/physics/08-implementation-roadmap-and-tests.md).

## 5. Checks after Changes

For code changes, first run affected subsystem tests, then the full suite when contracts span multiple subsystems. Development demos support observation and interaction and do not replace automated regression tests.

Documentation changes require link checks and, when complete examples are affected, example compilation. Validate the language-feature inventory using its statistics script:

```powershell
python scripts/check_documentation_links.py
python scripts/check_documentation_links.py --self-test
python scripts/check_documentation_parity.py
python scripts/check_documentation_parity.py --self-test
pwsh -File scripts/audit_cpp_language_features.ps1 -SelfTest
pwsh -File scripts/audit_cpp_language_features.ps1 -Language zh-CN -Check
pwsh -File scripts/audit_cpp_language_features.ps1 -Language en -Check
git diff --check
```

`python` requires Python 3; the statistics script requires PowerShell 7. Reports must distinguish compilation, executed tests, screenshot inspection, and human interaction. Historical acceptance counts cannot replace validation of current changes.

The default link command checks both language directories, both root READMEs, and incoming links from other languages; it does not fully check Japanese. Explicit paths remain supported, e.g. `python scripts/check_documentation_links.py docs/en README.md`. Coverage is local inline Markdown links, heading anchors, and fences, not external links, HTML images, or reference-style links. Parity checks relative paths, heading levels, fence languages/order, JSON contents, C++ tokens excluding comments/whitespace (preserving literals), and English reachability. Both compilation targets share one extraction list and generate separately by language; uncovered conceptual fragments are not claimed compiled. Feature statistics default to `-Language zh-CN`; use `-Language en` to generate and `-Language en -Update` to update English.
