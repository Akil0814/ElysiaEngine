# Building, Running, and Testing

This document explains how to build and run the repository's example program and tests. Run all commands from the repository root.

## Requirements

- CMake 3.24 or later.
- A C++23 compiler; MSVC x64 is recommended on Windows.
- Build tools for your generator, such as Ninja or Visual Studio MSBuild.
- Graphics hardware and drivers supporting the SDL3 GPU renderer to run examples and GPU tests.

Third-party dependency sources are bundled; ordinary configuration/build needs no dependency downloads. SDL3 is shared; SDL3_image, SDL3_ttf, SDL3_mixer, SDL3_gfx, and selected codecs are statically linked. See [dependency provenance](../../../thirdparty/SDL3-DEPENDENCIES.md) for versions and local patches.

## Windows Build and Run

### MSVC + Ninja

Install Visual Studio's Desktop development with C++ workload, CMake, and Ninja. Open **x64 Native Tools Command Prompt**, change to the repository root, and run:

```bat
cmake -S . -B out/build/msvc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/msvc-debug --parallel
.\out\build\msvc-debug\ElysiaEngine.exe
```

Use a separate Release directory:

```bat
cmake -S . -B out/build/msvc-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/msvc-release --parallel
.\out\build\msvc-release\ElysiaEngine.exe
```

Ninja selects configuration through `CMAKE_BUILD_TYPE`; executables reside directly in the build directory. The developer command environment determines MSVC x64 architecture.

### Visual Studio Generator

This example uses Visual Studio 2026 and its bundled CMake. For other versions, inspect available generators with `cmake --help` and replace the name after `-G`.

```powershell
cmake -S . -B out/build/msvc-vs -G "Visual Studio 18 2026" -A x64
cmake --build out/build/msvc-vs --config Debug --parallel
.\out\build\msvc-vs\Debug\ElysiaEngine.exe
```

Visual Studio is a multi-configuration generator: one build directory can build Debug and Release. Use `--config Release` when building and the corresponding `Release/` directory when running.

### Runtime Files

Launch from the repository root and retain the complete `assets/` directory, including `assets/.elysia_root` and `assets/content_registry.json`. On Windows, CMake copies the SDL3 DLL beside `application` and test executables after building.

## Common Build Options

| Option | Default | Purpose |
| --- | --- | --- |
| `ELYSIA_ENABLE_IMGUI` | `ON` | Build the Dear ImGui development overlay; toggle with F2 at runtime. |
| `BUILD_TESTING` | `ON` | Build and register tests; disable when only building the example. |

Append `-DELYSIA_ENABLE_IMGUI=OFF` or `-DBUILD_TESTING=OFF` to configuration commands to disable the corresponding feature. Both Debug and Release support enabling/disabling the overlay. See [development overlay](tools/development-panels.md) for interfaces.

## Running Tests

Tests are included by default. After building, use the corresponding build directory:

```powershell
ctest --test-dir out/build/msvc-debug --output-on-failure
```

Visual Studio also requires a configuration:

```powershell
ctest --test-dir out/build/msvc-vs -C Debug --output-on-failure
```

Filter by label or test name; these commands use the Ninja Debug directory:

```powershell
ctest --test-dir out/build/msvc-debug -L gpu --output-on-failure
ctest --test-dir out/build/msvc-debug -L input --output-on-failure
ctest --test-dir out/build/msvc-debug -L ui --output-on-failure
ctest --test-dir out/build/msvc-debug -R "^scene_lifecycle_regression_tests$" --output-on-failure
```

GPU tests require real graphics hardware and cannot use the dummy video driver. `sdl3_gpu_integration_tests` generates `sdl3-gpu-verification.png` in its working directory; the example `application` smoke test injects an exit event automatically.

`-LE '^gpu$'` excludes only tests labeled `gpu`. Application lifecycle branches requiring GPU carry both `application` and `gpu`; two scene-manager-only branches require no GPU. Label filtering does not guarantee that every remaining test can run without a display; check other device and filesystem requirements.

After editing guide examples, build the documentation example target separately; configuration must retain `BUILD_TESTING=ON`.

```powershell
cmake --build out/build/msvc-debug --target scene_documentation_examples_en
```

This target extracts selected complete examples directly from Markdown for compilation. It does not run the game or cover every conceptual fragment. See [testing and documentation example validation](../contributing/testing.md#3-compile-documentation-examples) for coverage and maintenance.

## Other Platforms

Linux, macOS, and MinGW use the same bundled dependencies and require their C++23 toolchain, build tools, and system graphics/audio development libraries. Linux Ninja configuration:

```bash
cmake -S . -B out/build/linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/linux-debug --parallel
./out/build/linux-debug/ElysiaEngine
ctest --test-dir out/build/linux-debug --output-on-failure
```

Use separate build directories and toolchains for macOS or MinGW. Builds and runtime behavior on other platforms still require separate validation; consult the bundled [SDL README](../../../thirdparty/SDL3/README.md) for system dependencies.

## Troubleshooting

- Missing `cmake`, `ninja`, or MSVC standard headers: check installation/PATH and use an x64 developer command prompt for MSVC + Ninja.
- Generator or architecture mismatch: use a new directory when changing generators, toolchains, or architecture.
- GPU creation failure: check graphics drivers and SDL GPU backend support; see [startup failures and troubleshooting](game-initialization.md#7-startup-failures-and-troubleshooting) for messages, handling, and checks.
- Missing resources, missing project root, or startup loading failure: [troubleshoot by startup stage](game-initialization.md#72-troubleshoot-common-errors-by-stage), distinguishing startup configuration, preloads, and project content loading.
- Missing SDL3 DLL: finish building and keep the DLL beside the executable; use the matching Debug or Release output.
