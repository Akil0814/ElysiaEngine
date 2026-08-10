# Overview

## Project Positioning

Moonline is a C++ 2D game project

The tracked C/C++ code in `engine/`, `game/`
is already about 25.6k LOC, excluding `thirdparty/`.

That means the older "around 25k LOC" estimate is no longer realistic as a final project-size
target. With the engine still unfinished and gameplay systems still relatively early, a more
reasonable final expectation is that the codebase will likely grow beyond 30k LOC and may land
closer to the mid-30k range depending on how much UI, save/load, combat, and data-pipeline work
is added.

The current source layout shows a clear separation between:

- `engine/application/`: reusable application startup, runtime ownership, and main loop
- `engine/`: reusable game engine foundation and its developer documentation
- `game/`: game-specific logic
- `assets/`: textures, audio, fonts, configs, preload resources
- `docs/`: project notes and planning documents

The current development priority is to complete the core engine and data pipeline first, then build gameplay systems on top of that foundation.

The engine developer documentation has a unified entry point at
[`engine/docs/`](../engine/docs/README.md).

## UI Documentation

The current `engine/ui` system has its own Chinese developer documentation in
[`engine/docs/ui/`](../engine/docs/ui/README.md). It includes a component-by-component public API reference,
shared API contracts, and a header coverage checklist.

## Input Documentation

The current Raw Input, Action Mapping, Engine Gameplay, and GameplayScene pipeline is documented
in Chinese in [`engine/docs/input/`](../engine/docs/input/README.md). It covers the implemented Button/Axis value model,
keyboard and controller bindings, runtime rebinding APIs, scene receiver lifecycle, and tests.

## Asset Configuration Documentation

The current resource-loading JSON chain, manifest schemas, entity content rules, animation paths,
and failure behavior are documented in Chinese in
[`engine/docs/json/`](../engine/docs/json/README.md).

AppConfig、用户持久化设置与通用游戏 ConfigService 的职责和现行 schema 见
[`engine/docs/runtime-config.md`](../engine/docs/runtime-config.md)。


## Planned Game Features

The intended target feature set currently includes:

- multi-stage progression
- support for multiple input schemes
- selectable player characters
- enemy system
- inventory / backpack system
- achievement system
- auto save
- manual save
- difficulty selection
- player progression, such as level-related growth
- drops and rewards tied to combat or stage completion

These systems are expected to share a common save data model and common gameplay state flow.

### Engine Responsibilities

The engine layer should eventually provide:

- application lifecycle
- scene switching and scene stack behavior if needed
- input abstraction
- resource loading and caching
- JSON/config loading
- audio playback management
- animation playback support
- base UI widgets and menu flow support
- save file IO support
- logging and runtime path management

### Gameplay Responsibilities

The gameplay layer should eventually provide:

- player runtime state
- enemy runtime state
- stage flow
- combat rules
- drop rules
- inventory logic
- achievement trigger logic
- difficulty modifiers
- save/load mapping between runtime state and persisted data

## Priority Development Areas

The next major work should focus on the engine rather than adding many end-user features immediately.

Recommended implementation order:

1. `ResourceManager` and related asset loading flow
2.  animation runtime support
3. adopt the existing Action Input and GameplayScene support in the minimum playable gameplay loop
4. config/data loaders for character, enemy, stage, and global JSON
5. audio management wrapper
6. save system base interfaces and save data schema
7. reusable UI components for menus and selection screens
8. minimum playable gameplay loop

## Minimum Playable Milestone

Before implementing all advanced systems, the project should aim for a minimum playable slice:

- enter the game
- load resources successfully
- show a main menu
- choose one character
- enter test scene
- allow movement and attack
- spawn one enemy type
- resolve damage and enemy defeat
- enter one game stage
- return to result or next-stage flow
- save and load basic progress

## Notes On Scope

Even if the final game direction changes later, many systems remain useful and should still be built carefully:

- save/load
- inventory
- achievements
- difficulty settings
- config-driven content
- UI flow
- input abstraction

The part most likely to change with genre direction is the combat core. For that reason, engine work should avoid overcommitting to
one combat implementation too early unless the direction is confirmed.
