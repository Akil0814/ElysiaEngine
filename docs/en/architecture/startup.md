# Complete Project Startup Flow

This diagram follows the current example GameModule initial_route from main to the main menu. Projects may select other initial and success routes; MainMenu is not a fixed engine entry point.

<a id="startup-flow"></a>
## Initialization and Runtime Phases

```mermaid
flowchart TB
  Entry["main<br/>Create GameModule"]
  InitFailure["Initialization failure<br/>Log diagnostics and show an error dialog"]
  Clean["Loading failure: stop and join workers<br/>Clear project content"]
  Failure["Currently enter ApplicationFailureScene<br/>A custom failure_route can be configured"]
  Shutdown["Application.shutdown<br/>Input / Scenes / Services / Resources<br/>Renderer / Window / TTF / MIX / SDL / Logging"]
  Exit["main returns EXIT_SUCCESS or EXIT_FAILURE"]
  subgraph InitPhase["Application.initialize · Synchronous initialization"]
  Descriptor["Logging, termination management, GameModule.descriptor<br/>Validate size, font settings and UI theme"]
  Bootstrap["Bootstrapper.parse_runtime_settings<br/>Paths and directories → Registry → App and user config"]
  Save["File logging / SaveService"]
  Platform["initialize_runtime<br/>SDL → MIX → TTF → AudioService<br/>Window, GPU renderer and display settings"]
  Dev["Optional development overlay"]
  Builtins["BuiltinResources → BuiltinMusicPlayer<br/>LocalizationManager → FontResolver"]
  Preload["Settings callbacks → Input → Startup texture preload"]
  Compose["SceneRuntimeContext / SceneManager<br/>Register engine, Realm and game scenes"]
  Enter["SceneManager.start initial route<br/>StartupLoadingScene.on_enter<br/>Create UI, start logos and GameContentLoader.start"]
  end
  subgraph RunPhase["Application.run · Main loop"]
  Loop["Events / Input → Scene and audio updates<br/>Render / Present → Frame pacing"]
  Logo["Logo presentation<br/>Engine → Optional project logo"]
  Load["GameContentLoader.update<br/>Plan → Resource commits → Animation and effect registration<br/>Publish ConfigSnapshot"]
  Fonts["Activate project fonts"]
  Gate["StartupLoadingCompletion<br/>Loading complete; optionally wait for logos"]
  Confirm["Current configuration requires confirmation<br/>wait_for_confirmation = true"]
  Leave["StartupLoadingScene.on_exit<br/>Reset loader, destroy UI, release preload<br/>Keep successfully loaded project content"]
  Menu["MainMenuScene<br/>Startup complete; continue in the same main loop"]
  end
  Entry -->|initialize| Descriptor
  Descriptor -->|Continue initialization| Bootstrap
  Bootstrap -->|Provide paths and runtime settings| Save
  Save -->|Continue| Platform
  Platform -->|Continue| Dev
  Dev -->|Continue| Builtins
  Builtins -->|Continue| Preload
  Preload -->|Base dependencies ready| Compose
  Compose -->|start initial_route| Enter
  Enter -->|initialize succeeds; main calls run| Loop
  Loop -->|Advance presentation each frame| Logo
  Loop -->|Advance each frame; workers prepare in parallel| Load
  Load -->|Loading succeeds| Fonts
  Fonts -->|Activation succeeds| Gate
  Logo -.->|Contributes when waiting is enabled; currently false| Gate
  Gate -->|Current confirmation policy| Confirm
  Gate -.->|Switch directly when confirmation is disabled| Leave
  Confirm -->|User confirms; switch to success_route| Leave
  Leave -->|Enter target scene| Menu
  Bootstrap -.->|Startup configuration fails; other initialization failures also abort| InitFailure
  InitFailure -->|Abort startup| Shutdown
  Load -->|Loading fails| Clean
  Clean -->|Request failure route| Failure
  Fonts -->|Activation fails| Failure
  Enter -.->|Startup UI creation fails| Failure
  Loop -.->|Unrecoverable exception / Rendering error| Shutdown
  Failure -->|User selects exit| Shutdown
  Menu -->|Exit application| Shutdown
  Shutdown -->|Based on run and cleanup results| Exit
```

Source: [main.cpp](../../../main.cpp) · [application.cpp](../../../engine/application/application.cpp) · [application_scene_composition.cpp](../../../engine/application/composition/application_scene_composition.cpp) · [bootstrapper.cpp](../../../engine/bootstrap/bootstrapper.cpp) · [startup_loading_scene.cpp](../../../engine/builtin/scenes/startup_loading_scene.cpp)

## Current Example and Engine Options

The example enters StartupLoading and then MainMenu on success, with wait_for_logo_sequence=false and wait_for_confirmation=true, and no project logo or custom failure route. The Payload default for waiting on logos is true. Disabling the wait does not stop logo playback; disabling confirmation permits a direct transition.

Source: [example_game_module.cpp](../../../game/application/example_game_module.cpp) · [startup_loading_scene_payload.h](../../../engine/builtin/scenes/startup_loading_scene_payload.h) · [startup_loading_flow.h](../../../engine/builtin/scenes/startup_loading_flow.h)

## Timing and Failure Boundaries

- initialize enters the initial scene. on_enter synchronously prepares manifests and the plan and starts workers; run then advances loading and presentation, without waiting for all project content before entering the main loop.
- Workers prepare CPU data only; GPU creation and commits occur on the main thread. See the [resource service diagram](subsystems/resources/README.md#resource-service-architecture).
- Startup configuration, platform or required preload failure aborts initialization. Ordinary initialization failures use logging and dialogs; unrecoverable rendering errors use the termination boundary.
- Content failure clears workers and project content before requesting failure_route or the built-in failure scene; font activation and startup presentation failures also use failure routing. SceneManager attempts error-scene recovery for ordinary scene lifecycle exceptions and terminates if recovery is unavailable.
- Leaving the startup scene releases preload caches while keeping successfully loaded project content. Application shutdown ends users and playback before releasing assets and platform facilities; cleanup failure affects the exit result.

Solid arrows show the main flow; dashed arrows show conditional or exceptional branches. [Startup configuration guide](../user-guide/builtin-scenes/startup-loading.md) · [Service overview](services.md) · [Scene lifecycle](subsystems/scene-lifecycle.md).
