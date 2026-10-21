# C++11–23 Language and Library Feature Statistics

This page is for developers studying the project's language usage or maintaining its scanner. It explains which features can be identified reliably in source, how to reproduce the statistics, and what the numbers do not establish.

It inventories reproducibly identifiable modern C++ usage in first-party Elysia Engine code. The [CMake configuration](../../../CMakeLists.txt) requires C++23.

## Scope and Counting Rules

- Scan `.h`, `.hpp`, and `.cpp` files under `engine/`, `game/`, and `tests/`.
- Exclude `thirdparty/`, build directories, documentation, scripts, and generated artifacts.
- Hide comments, ordinary/raw strings, and character literals before matching registered syntax and library symbols.
- The `engine`, `game`, and `tests` columns count pattern matches per directory; “Files” counts files with at least one match.
- A line may demonstrate several features: `constexpr auto` contributes to both `constexpr` and `auto`.
- Counts describe lexical/syntactic patterns, not runtime calls, performance weight, or code quality.

## Current Statistics

<!-- BEGIN GENERATED: cpp-language-feature-stats -->

Generated from the current working tree by `scripts/audit_cpp_language_features.ps1`; 763 first-party C++ source files scanned.
A match counts a detector pattern after excluding comments and string/character literals, not runtime calls.

### C++11

| ID | Category | Feature | engine | game | tests | Total matches | Files | Representative location |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp11-array` | Library | `std::array` | 59 | 42 | 81 | 182 | 85 | [Source](../../../game/showcase/ui/hud_demo_state.h) |
| `cpp11-auto` | Language | `auto` type deduction | 1340 | 461 | 1554 | 3355 | 309 | [Source](../../../game/navigation/main_menu_scene.cpp) |
| `cpp11-chrono` | Library | `std::chrono` | 38 | 1 | 18 | 57 | 26 | [Source](../../../tests/audio/sound_playback_scheduler_tests.cpp) |
| `cpp11-concurrency` | Library | Threads, synchronization, and atomics | 55 | 0 | 20 | 75 | 12 | [Source](../../../engine/tools/termination_manager.h) |
| `cpp11-constexpr` | Language | `constexpr` | 533 | 43 | 56 | 632 | 128 | [Source](../../../engine/application/application.cpp) |
| `cpp11-decltype` | Language | `decltype` | 14 | 2 | 4 | 20 | 15 | [Source](../../../tests/camera/camera_manager_tests.cpp) |
| `cpp11-declval` | Library | `std::declval` | 0 | 0 | 7 | 7 | 2 | [Source](../../../tests/camera/camera_manager_tests.cpp) |
| `cpp11-defaulted-functions` | Language | Explicitly defaulted functions | 168 | 6 | 0 | 174 | 135 | [Source](../../../game/navigation/main_menu_scene.h) |
| `cpp11-deleted-functions` | Language | Explicitly deleted functions | 81 | 4 | 0 | 85 | 28 | [Source](../../../engine/camera/camera_manager.h) |
| `cpp11-enum-class` | Language | Scoped enumerations | 163 | 11 | 3 | 177 | 109 | [Source](../../../engine/animation/animation_registration_failure.h) |
| `cpp11-exception-pointers` | Library | Exception capture and rethrow | 59 | 0 | 2 | 61 | 12 | [Source](../../../tests/application/application_startup_exception_tests.cpp) |
| `cpp11-final` | Language | `final` | 65 | 46 | 115 | 226 | 136 | [Source](../../../game/showcase/ui/ui_component_gallery_scene.h) |
| `cpp11-fixed-width-integers` | Library | Fixed-width integer types | 342 | 13 | 71 | 426 | 132 | [Source](../../../engine/audio/sound_playback_types.h) |
| `cpp11-function` | Library | `std::function` | 64 | 57 | 31 | 152 | 79 | [Source](../../../engine/animation/animation.h) |
| `cpp11-initializer-list` | Library | `std::initializer_list` | 9 | 1 | 1 | 11 | 10 | [Source](../../../engine/resources/atlas/atlas.h) |
| `cpp11-lambda` | Language | Lambda expressions | 502 | 187 | 445 | 1134 | 181 | [Source](../../../game/showcase/effects/effects_showcase_scene.cpp) |
| `cpp11-math-functions` | Library | C++11 mathematical functions | 205 | 14 | 5 | 224 | 51 | [Source](../../../engine/application/application.cpp) |
| `cpp11-move-forward` | Library | Move and perfect forwarding utilities | 854 | 222 | 183 | 1259 | 223 | [Source](../../../game/navigation/main_menu_scene.cpp) |
| `cpp11-new-algorithms` | Library | C++11 non-modifying sequence algorithms | 16 | 0 | 3 | 19 | 9 | [Source](../../../engine/ui/window/ui_window.cpp) |
| `cpp11-noexcept` | Language | `noexcept` | 3123 | 131 | 74 | 3328 | 394 | [Source](../../../engine/scene/scene_manager.h) |
| `cpp11-nullptr` | Language | `nullptr` | 641 | 123 | 480 | 1244 | 259 | [Source](../../../engine/scene/scene_manager.h) |
| `cpp11-override` | Language | `override` | 383 | 129 | 377 | 889 | 140 | [Source](../../../game/showcase/ui/ui_component_gallery_scene.h) |
| `cpp11-quick-exit` | Library | `std::_Exit` | 0 | 0 | 3 | 3 | 3 | [Source](../../../tests/application/application_startup_exception_tests.cpp) |
| `cpp11-random` | Library | Random number utilities | 2 | 0 | 0 | 2 | 2 | [Source](../../../engine/tools/random_generator.cpp) |
| `cpp11-range-for` | Language | Range-based `for` | 415 | 69 | 179 | 663 | 193 | [Source](../../../game/showcase/ui/ui_component_gallery_content.cpp) |
| `cpp11-reference-wrappers` | Library | Reference wrappers | 0 | 0 | 17 | 17 | 6 | [Source](../../../tests/scene/scene_recovery_tests.cpp) |
| `cpp11-smart-pointers` | Library | Smart pointers and `std::make_shared` | 194 | 46 | 25 | 265 | 113 | [Source](../../../engine/ui/core/ui_child_host.h) |
| `cpp11-static-assert` | Language | `static_assert` | 13 | 0 | 49 | 62 | 20 | [Source](../../../engine/scene/scene_manager.h) |
| `cpp11-string-conversion` | Library | String and numeric conversions | 32 | 19 | 35 | 86 | 43 | [Source](../../../game/showcase/gameplay/runtime/demo_combat.cpp) |
| `cpp11-system-error` | Library | System error code utilities | 56 | 0 | 28 | 84 | 21 | [Source](../../../engine/builtin/resources/builtin_asset_catalog.cpp) |
| `cpp11-time-formatting` | Library | `std::put_time` | 2 | 0 | 0 | 2 | 1 | [Source](../../../engine/tools/logger.cpp) |
| `cpp11-tuple` | Library | Tuple utilities | 6 | 0 | 0 | 6 | 4 | [Source](../../../engine/ui/core/ui_child_host.h) |
| `cpp11-type-alias` | Language | `using` type aliases | 123 | 10 | 12 | 145 | 92 | [Source](../../../engine/animation/animation.h) |
| `cpp11-type-index` | Library | `std::type_index` | 3 | 0 | 0 | 3 | 2 | [Source](../../../engine/ui/style/ui_theme_style_resolver.h) |
| `cpp11-unordered-containers` | Library | Unordered containers and hashing | 65 | 3 | 2 | 70 | 39 | [Source](../../../engine/builtin/resources/builtin_asset_cache.h) |
| `cpp11-variadic-templates` | Language | Variadic templates | 10 | 2 | 0 | 12 | 6 | [Source](../../../engine/ui/core/ui_child_host.h) |

### C++14

| ID | Category | Feature | engine | game | tests | Total matches | Files | Representative location |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp14-chrono-literals` | Library | Chrono literals | 0 | 1 | 3 | 4 | 4 | [Source](../../../tests/audio/sound_playback_scheduler_tests.cpp) |
| `cpp14-exchange` | Library | `std::exchange` | 6 | 1 | 11 | 18 | 11 | [Source](../../../engine/gameplay/control/controller_manager.cpp) |
| `cpp14-generic-lambda` | Language | Generic lambdas | 79 | 9 | 61 | 149 | 53 | [Source](../../../game/showcase/gameplay/gameplay_demo_scene_base.cpp) |
| `cpp14-init-capture` | Language | Lambda init-capture | 2 | 4 | 0 | 6 | 5 | [Source](../../../engine/ui/widgets/ui_button.cpp) |
| `cpp14-make-unique` | Library | `std::make_unique` | 75 | 156 | 185 | 416 | 73 | [Source](../../../engine/animation/animation_service.cpp) |
| `cpp14-type-trait-aliases` | Library | Type trait `_t` aliases | 15 | 2 | 2 | 19 | 12 | [Source](../../../engine/ui/core/ui_child_host.h) |

### C++17

| ID | Category | Feature | engine | game | tests | Total matches | Files | Representative location |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp17-any` | Library | `std::any` | 2 | 0 | 1 | 3 | 2 | [Source](../../../tests/scene/scene_core_tests.cpp) |
| `cpp17-apply` | Library | `std::apply` | 1 | 0 | 0 | 1 | 1 | [Source](../../../engine/scene/scene_manager.h) |
| `cpp17-as-const` | Library | `std::as_const` | 3 | 0 | 0 | 3 | 1 | [Source](../../../engine/object_query/game_object_query_service.h) |
| `cpp17-clamp` | Library | `std::clamp` | 70 | 3 | 0 | 73 | 37 | [Source](../../../engine/camera/camera_controller.cpp) |
| `cpp17-ctad` | Language | Class template argument deduction | 5 | 5 | 44 | 54 | 26 | [Source](../../../tests/application/application_presentation_settings_tests.cpp) |
| `cpp17-filesystem` | Library | `std::filesystem` | 372 | 0 | 371 | 743 | 134 | [Source](../../../engine/io/path/path_manager.h) |
| `cpp17-if-constexpr` | Language | `if constexpr` | 43 | 2 | 0 | 45 | 14 | [Source](../../../engine/camera/camera_manager.cpp) |
| `cpp17-if-initializer` | Language | `if` initializer statements | 147 | 3 | 13 | 163 | 70 | [Source](../../../engine/animation/runtime/animation_manager.cpp) |
| `cpp17-inline-variables` | Language | Inline variables | 234 | 24 | 0 | 258 | 21 | [Source](../../../engine/core/render/colors.h) |
| `cpp17-invoke` | Library | `std::invoke` and result type traits | 9 | 0 | 0 | 9 | 2 | [Source](../../../engine/object_query/game_object_query_service.h) |
| `cpp17-maybe-unused` | Language | `[[maybe_unused]]` | 19 | 0 | 0 | 19 | 3 | [Source](../../../engine/scene/scene.h) |
| `cpp17-nested-namespace` | Language | Nested namespace definitions | 586 | 155 | 98 | 839 | 682 | [Source](../../../engine/animation/animation.h) |
| `cpp17-nodiscard` | Language | `[[nodiscard]]` | 1617 | 78 | 45 | 1740 | 305 | [Source](../../../engine/animation/animation.h) |
| `cpp17-optional` | Library | `std::optional` / `std::nullopt` | 556 | 37 | 41 | 634 | 190 | [Source](../../../engine/animation/animation.h) |
| `cpp17-scoped-lock` | Library | `std::scoped_lock` | 7 | 0 | 0 | 7 | 1 | [Source](../../../engine/config/config_service.cpp) |
| `cpp17-size` | Library | `std::size` | 0 | 0 | 1 | 1 | 1 | [Source](../../../tests/loading/resource_load_plan_validator_tests.cpp) |
| `cpp17-string-view` | Library | `std::string_view` | 471 | 17 | 118 | 606 | 159 | [Source](../../../engine/animation/animation_service.h) |
| `cpp17-structured-bindings` | Language | Structured bindings | 68 | 3 | 6 | 77 | 31 | [Source](../../../game/showcase/gameplay/runtime/demo_combat.cpp) |
| `cpp17-type-trait-variables` | Library | Type trait `_v` variable templates | 42 | 2 | 21 | 65 | 25 | [Source](../../../engine/camera/camera_manager.cpp) |
| `cpp17-variant` | Library | Variant utilities | 63 | 2 | 11 | 76 | 33 | [Source](../../../engine/ui/widgets/ui_button.cpp) |

### C++20

| ID | Category | Feature | engine | game | tests | Total matches | Files | Representative location |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp20-bit-operations` | Library | Bit utilities | 1 | 0 | 0 | 1 | 1 | [Source](../../../engine/tools/debug_draw.cpp) |
| `cpp20-concepts` | Language | Concepts and `requires` clauses | 15 | 0 | 3 | 18 | 5 | [Source](../../../engine/save/save_data.h) |
| `cpp20-designated-initializers` | Language | Designated initializers | 91 | 84 | 458 | 633 | 119 | [Source](../../../game/navigation/main_menu_scene.cpp) |
| `cpp20-erase` | Library | `std::erase` / `std::erase_if` | 45 | 2 | 0 | 47 | 18 | [Source](../../../engine/physics/physics_world.cpp) |
| `cpp20-format` | Library | `std::format` | 0 | 4 | 0 | 4 | 1 | [Source](../../../game/showcase/camera/camera_showcase_scene.cpp) |
| `cpp20-jthread` | Library | `std::jthread` / stop tokens | 0 | 0 | 4 | 4 | 2 | [Source](../../../tests/application/application_lifecycle_tests.cpp) |
| `cpp20-lerp` | Library | `std::lerp` | 1 | 0 | 0 | 1 | 1 | [Source](../../../engine/gameplay/ui/world_bar.cpp) |
| `cpp20-numbers` | Library | Mathematical constants | 1 | 0 | 0 | 1 | 1 | [Source](../../../engine/physics/physics_debug_draw.cpp) |
| `cpp20-ordering-types` | Library | Comparison category types | 4 | 0 | 0 | 4 | 2 | [Source](../../../engine/physics/collision/collision_target.h) |
| `cpp20-ranges` | Library | Ranges library | 42 | 4 | 5 | 51 | 23 | [Source](../../../engine/physics/physics_world.cpp) |
| `cpp20-remove-cvref` | Library | `std::remove_cvref_t` | 19 | 0 | 0 | 19 | 6 | [Source](../../../engine/save/save_service.h) |
| `cpp20-source-location` | Library | `std::source_location` | 197 | 0 | 19 | 216 | 69 | [Source](../../../engine/application/lifecycle/application_termination_logging.h) |
| `cpp20-span` | Library | `std::span` | 60 | 7 | 13 | 80 | 52 | [Source](../../../engine/builtin/object/engine_character.cpp) |
| `cpp20-standard-concepts` | Library | Standard library concepts | 37 | 0 | 0 | 37 | 5 | [Source](../../../engine/object_query/game_object_query_service.h) |
| `cpp20-string-prefix-suffix` | Library | `starts_with` / `ends_with` | 10 | 0 | 15 | 25 | 11 | [Source](../../../tests/save/save_service_tests.cpp) |
| `cpp20-three-way-comparison` | Language | Three-way comparison | 16 | 0 | 0 | 16 | 7 | [Source](../../../engine/physics/collision/collision_target.h) |

### C++23

| ID | Category | Feature | engine | game | tests | Total matches | Files | Representative location |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp23-expected` | Library | `std::expected` / `std::unexpected` | 1107 | 0 | 35 | 1142 | 141 | [Source](../../../engine/bootstrap/bootstrapper.h) |

### Common Features Not Currently Detected

| Standard | ID | Category | Feature |
| --- | --- | --- | --- |
| C++14 | `cpp14-binary-literals` | Language | Binary literals |
| C++14 | `cpp14-digit-separators` | Language | Digit separators |
| C++17 | `cpp17-fallthrough` | Language | `[[fallthrough]]` |
| C++20 | `cpp20-bit-cast` | Library | `std::bit_cast` |
| C++20 | `cpp20-consteval` | Language | `consteval` |
| C++20 | `cpp20-constinit` | Language | `constinit` |
| C++20 | `cpp20-coroutines` | Language | Coroutines |
| C++23 | `cpp23-byteswap` | Library | `std::byteswap` |
| C++23 | `cpp23-if-consteval` | Language | `if consteval` |
| C++23 | `cpp23-mdspan` | Library | `std::mdspan` |
| C++23 | `cpp23-print` | Library | `std::print` / `std::println` |
| C++23 | `cpp23-to-underlying` | Library | `std::to_underlying` |

<!-- END GENERATED: cpp-language-feature-stats -->

## Use the Scanner

Run from the repository root with PowerShell 7 or later:

```powershell
# Print the complete Markdown statistics block without changing files
.\scripts\audit_cpp_language_features.ps1 -Language en

# Regenerate this page's statistics block
.\scripts\audit_cpp_language_features.ps1 -Language en -Update

# Check this page against current source
.\scripts\audit_cpp_language_features.ps1 -Language en -Check

# List all matches for a feature
.\scripts\audit_cpp_language_features.ps1 -Language en -Feature cpp20-span

# Verify filtering, detection, and symbol classification
.\scripts\audit_cpp_language_features.ps1 -Language en -SelfTest
```

`-Check` also verifies that each used feature has a valid representative source and that every encountered `std::` identifier is classified in the modern-feature registry or the explicit older/common-library baseline. Maintainers must establish the standard version of new symbols before statistics can pass.

## Limits of Automatic Statistics

The scanner deliberately avoids false precision for these semantic features:

- Guaranteed copy elision, implicit moves, and return-value optimization, whose behavior depends on semantics and the compiler.
- Aggregate initialization, uniform initialization, and class template argument deduction that cannot be attributed reliably from local text; currently only recognizable `std::array` CTAD is counted.
- Compiler extensions, post-preprocessing code, and usage inside third-party headers.
- Indirect usage that requires overload resolution, template instantiation, or type-system reasoning.

“Comprehensive” therefore means covering registered C++11–23 feature families reliably recognizable in current first-party source and listing common candidates not currently detected. It is not an ISO C++ clause-by-clause conformance matrix.

## Maintenance

For a new detector, register a stable ID, standard version, category, pattern, and representative source in the script, and extend `-SelfTest`. If `-Check` reports stale statistics after a source change, run `-Update`, review count changes and representative links, then commit the document. Coding decisions still follow the [C++ Coding Standard](coding-standard.md).
