<div align="center">
  <img src="assets/engine/textures/elysia.png" alt="Elysia Engine" width="300">
  <h1>Elysia Engine</h1>
</div>

**[English](README.md)** | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

Elysia Engine is a modular 2D game engine and runtime framework built with C++23 and SDL3. It provides common game capabilities including application lifecycle management, scene routing, resource management, input, rendering, audio, UI, physics, and save data.

This repository contains both the reusable engine implementation and a sample application: `engine/` provides the engine capabilities, while `game/` demonstrates game module integration. The built `ElysiaEngine` executable runs this sample application. The engine implementation and game logic remain separate so each can be extended and maintained independently.

## Core Features and Current Status

- **Application and scenes**: Application lifecycle management, scene registration and routing, and game module integration boundaries.
- **Resources and configuration**: Resource manifests, content registration, runtime configuration, internationalization, and user configuration services.
- **Input and Gameplay**: Raw device input, action mapping, player controllers, command consumption on fixed ticks, and scene input routing.
- **Rendering and UI**: SDL3 GPU renderer, retained-mode UI, layout, focus, window surfaces, styles, and widgets.
- **Audio and cameras**: Audio resource playback scheduling, camera slots, following, screen shake, and coordinate projection.
- **Physics and save data**: Box2D integration, collision and Gameplay runtime support, typed save data, and reliable recovery.
- **Animation and effects**: Animation resources, atlases, and `EffectDefinition` configuration.
- **Development support**: Demo Gallery, an optional Dear ImGui development overlay, and unit and integration tests organized by subsystem.

**Networking limitations**: ENet is currently retained only as a transport-layer dependency linked by the engine. Network sessions, state synchronization, and networked multiplayer have not been implemented.

## Projects Using the Engine and Project History

### Projects Using the Engine

| Project | Description | Status |
| --- | --- | --- |
| [Codex Zero](https://github.com/ZacharyOllivierre/Codex-Zero) | A 2D top-down multiplayer shooter built with this engine | In development |
| [Moonline](https://github.com/Akil0814/Moonline) | A 2D side-view fighting game featuring characters from MELTY BLOOD | Development stalled |

### Project History

- Early engine development commits: [Moonline](https://github.com/Akil0814/Moonline).
- Early prototype development: [Project-Hail-Mary](https://github.com/ZacharyOllivierre/Project-Hail-Mary).


## Documentation

| Purpose | Documentation |
| --- | --- |
| Set up the environment, build, run, and troubleshoot | [Build, Run, and Test](docs/en/user-guide/build-and-run.md) |
| Find guidance on using the engine and its public interfaces | [User Guide](docs/en/user-guide/README.md) |
| Understand subsystem boundaries and runtime behavior | [Architecture and Module Design](docs/en/architecture/README.md) |
| Contribute and learn about coding conventions, testing, and documentation maintenance | [Development and Contribution Guide](docs/en/contributing/README.md) |
| Explore planned capabilities, technical discussions, and migration designs | [Design and Roadmap](docs/en/design/README.md) |
| Browse developer documentation categories | [Developer Documentation Index](docs/en/README.md) |

## Repository Structure and Dependency Boundaries

- `engine/`: Reusable engine implementation, built as `engine_lib`.
- `game/`: Sample integration layer providing a game module and sample scenes, built as `game_lib`.
- `assets/`: Configuration, fonts, audio, and textures used by the sample application and built-in engine flows.
- `tests/`: Unit and integration tests organized by subsystem.
- `docs/`: User guides, architecture descriptions, contribution guides, and design documents.
- `thirdparty/`: Third-party dependencies bundled with the repository.

The main dependency direction is:

```text
ElysiaEngine executable -> game_lib -> engine_lib
                                      -> SDL3 libraries
                                      -> Box2D
                                      -> ENet
```

The engine layer does not depend on `game/`. Game logic provides the application description and scene registration through `IGameModule`, and game-specific types are maintained by the game module.


## Quick Start

### Requirements

The following workflow uses Windows, MSVC x64, and a Visual Studio multi-configuration generator. It requires:

- CMake 3.24 or later.
- An MSVC toolchain with C++23 support.
- A graphics device and driver that support the SDL3 GPU renderer. If GPU renderer creation fails, the application reports an error and exits without automatically falling back to software rendering.

SDL3, image, ttf, mixer, gfx, and the required codecs are all built from fixed source versions bundled in the repository. Normal configuration and builds do not download dependencies. See [Dependency Sources](thirdparty/SDL3-DEPENDENCIES.md) for versions and local patches.

### Build and Run

Run from the repository root:

```powershell
cmake -S . -B out/build/sdl3-Debug -A x64
cmake --build out/build/sdl3-Debug --config Debug --parallel
.\out\build\sdl3-Debug\Debug\ElysiaEngine.exe
```

Keep the working directory at the repository root when running the application, and preserve the `assets/` directory structure so resources can load correctly.

The Dear ImGui development overlay is enabled by default. See [Build, Run, and Test](docs/en/user-guide/build-and-run.md) for instructions on disabling it, configuring Release builds, and building with Ninja.

> When migrating from an older version, do not reuse an SDL2 build cache; use a new SDL3 build directory. Use separate directories when switching generators or architectures as well.

### Run Tests

After building, run:

```powershell
ctest --test-dir out/build/sdl3-Debug -C Debug --output-on-failure
```

GPU tests require a real graphics device and cannot use the dummy video driver. See [Build, Run, and Test](docs/en/user-guide/build-and-run.md) for running tests by label, testing without a display, and troubleshooting.

### Other Platforms

Linux, macOS, and MinGW use the same fixed source dependencies bundled in the repository, but have not yet been verified on actual systems. Windows build results do not constitute verification of other platforms. See the [Detailed Build Instructions](docs/en/user-guide/build-and-run.md) for toolchain and system dependency requirements.

## License and Copyright

The project code is licensed under the [MIT License](LICENSE). Licenses and notices for bundled third-party source code are listed in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md); that code remains subject to its respective license terms.

The Elysia icon is copyrighted © miHoYo and is not covered by this project's MIT license grant.

Elysia Engine is an independent project and is not officially affiliated with, endorsed by, or sponsored by miHoYo.
