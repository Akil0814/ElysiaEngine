# Module Showcase Application

This document describes source ownership, return routes, and state cleanup for demo maintainers. The application combines and demonstrates engine capabilities; its business types do not enter the engine library.

`game_lib` is the project-owned engine showcase application. The main menu retains module demos, settings, and exit. The gallery orders UI, input/control, cameras, animation, effects, audio, physics, and Gameplay. Elysia Realm and confirmed fault injection reside in the auxiliary area.

## Files and Responsibilities

- `game/application` explicitly registers scenes and configures startup routes.
- `game/navigation` holds the main menu and scene identifiers. Existing values remain stable; audio, the Gameplay catalog, and Gameplay validation use 15, 16, and 17.
- `game/showcase/<module>` keeps scenes, views, state, and runtime logic close together. Scenes manage input, routes, and lifecycles; views build UI, display data, and invoke operation callbacks.
- `game/showcase/shared` holds shells, geometry layouts, and Inspector components actually shared by modules.
- `game/showcase/scenarios` holds shared scenario execution, validation display, and catalog organization. Combat actors, sessions, maps, and obstacles live under `game/showcase/gameplay/runtime`.

Scenes still own windows and `SceneObject`s. View control pointers are borrowed. Before rebuilding, clear theme registrations and references; before exit, detach Inspector and restore device/debug state. `destroy()` only marks objects; scene safe points release them.

## Routes and Presentation

Ordinary modules and free play use `ShowcaseEnterPayload{return_route}`. Validation uses `ScenarioEnterPayload{return_route, scenario_id, pressure_tier}`. Preserve target, payload, and reload mode of the entire return route.

Catalog, UI, camera, animation, and effect scenes allow cache reuse. Local multiplayer, audio, free play, and validation are recreated when entered from the catalog. Restarting validation recreates the scenario; pause and single-step do not create a second world.

`ShowcaseFrame` supplies consistent title, description, return, and status areas. List pages use scrolling content; world demos use a lightweight shell with independent controls. UI Gallery retains pages. `HudDemoState` stores skill cooldown and item state; `HudDemoView` presents controls. Hidden pages do not process shortcuts.

## Camera Showcase

`CameraShowcaseScene` uses identifier 13 and owns Main and Cinematic. Follow, motion, and cinematic pages share two actors and a world display. The scene stores `CameraDemoState`, handles, and phases; ControlsView receives data/callbacks, and OverlayView receives projections from the current presentation camera. Tabs use `UiTabBar`; operation rows each have a scroll container and focus scope.

Cinematic phases advance through blend/motion completion value events with handle checks. Hold timing skips the path-completion frame to avoid counting a whole frame's delta twice. Actor control gating freezes movement while retaining bindings and saved auto-motion settings. Exit and failed entry clear activity and borrowed references; controls release at scene safe points.

`SceneCameraRuntime` retains ownership of a terminal Holding handle. Pausing that motion also freezes the slot's shake presentation. Public interfaces remain unchanged.

Results and outstanding experience checks are in the [camera showcase record](camera-showcase-validation.md).

## Physics and Gameplay

Physics offers ten basic cases and low/medium/high tiers for bodies, contacts, and tiles stress cases. Gameplay offers collision, platform, and top-down combat scenes and three corresponding combat validation cases.

`ShowcaseScenario` is shared by demos and CTest. Existing case IDs, step counts, and thresholds are retained. Physics validation owns only its scenario world; free play owns only the `GameplayScene` world. Gameplay validation uses identifier 17 rather than switching Verify/FreePlay branches in one scene.

Gameplay controls provide pause, single-step, restart, world-UI visibility, bubble triggers, and return. Actors' existing `WorldBar`, `WorldText`, and `SpeechBubble` components anchor to presentation positions. Damage, death, and cleanup retain existing combat rules.

## Audio

Run `python scripts/generate_showcase_audio.py` to regenerate quiet PCM WAV demo assets. They require no external audio. The project manifest registers one sound and two looping music tracks.

The demo covers immediate/delayed sounds, loops, group limits/cooldown, music fades/transitions, and volume. Scenes retain their own SoundHandles and cancel those active or scheduled requests on exit; other scenes' requests are not globally canceled. Volume and group configuration are restored on exit and failed entry, without writing user configuration. Demo music uses the engine's single music playback channel.

## Validation

Tests live in `tests/game`; showcase code is neither stored there nor compiled into `engine_lib`. Existing CTest case names remain unchanged. New `showcase_tests` checks module order, case ownership, audio assets, and exit restoration. `showcase_render_tests` loads project assets and traverses modules, gameplay, validation, and five languages.

Automated coverage includes full return routes, repeated entry, HUD keyboard/mouse interaction, camera re-entry, multiplayer device restoration, Inspector unregistration, Gameplay world UI, and basic/combat cases. Software screenshots check layout; real keyboard/mouse/gamepad experience and audio listening require separate records.

Execution results and outstanding manual checks are in the [showcase refactoring record](showcase-validation.md).
