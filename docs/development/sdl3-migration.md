# SDL3 migration acceptance record

Status: SDL3 migration accepted on Windows MSVC x64. On 2026-09-14, the user confirmed that all functionality is normal and authorized removal of the old files. The retained SDL2 dependency and unused ImGui SDL2 backends have been removed. Linux, macOS and MinGW remain unverified.

## Implementation

- SDL 3.4.16, image 3.4.6, ttf 3.2.2, mixer 3.2.4 and the pinned community SDL3_gfx snapshot build from local source. [Sources and patches](../../thirdparty/SDL3-DEPENDENCIES.md).
- Application startup creates the SDL3 GPU renderer and reports its GPU backend. Failure is explicit. SDL3 lifecycle, boolean results, window presentation, texture filtering and ImGui 1.92.9 backends are migrated.
- Renderer commands retain SDL_Texture pointers, integer texture destinations, float world primitives, layer ordering and existing stroke geometry. SDL3 vertex colors are normalized floats.
- Input translates raw SDL3 events after ImGui processing. Logical mouse positions are rounded at the conversion boundary; wheel events retain the existing cached-pointer contract. Gamepads use SDL device IDs. Text input and IME rectangles use the focused SDL window.
- Audio uses one mixer, 24 sound tracks and one music track. Resource keys, groups, handles, loops, scheduler timing and fade controllers are retained. Sound is predecoded; music is streamed. Cache unload stops and detaches any referencing tracks before releasing MIX_Audio.
- Active first-party source and CMake targets no longer depend on SDL2. The old SDL2 vendor directory and the four unused ImGui SDL2 backend source/header files were removed after user acceptance.

## Baseline and evidence

Implementation began with a clean Git working tree. The old cached build had a PDB conflict; a fresh SDL2 Debug build using /Z7 completed and passed 84/84 tests. Baseline log: `out/sdl3-migration/baseline-clean-ctest.log`. No pre-migration UI screenshot baseline was captured, so side-by-side visual equivalence is not claimed.

Validation on 2026-09-14 uses MSVC x64, Ninja and independent SDL3 build directories:

| Configuration | Build | CTest |
| --- | --- | --- |
| Debug, ImGui ON | Passed | 87/87 passed |
| Release, ImGui ON | Passed | 87/87 passed |
| Release, ImGui OFF | Passed | 86/86 passed |

Logs are under `out/sdl3-migration/`, with `ctest-debug.log`, `ctest-release.log`, and `ctest-noimgui.log`. The full suite includes GPU tests; do not substitute a software-only run.

The actual GPU backend is Direct3D 12. Integration checks exercise invalid-backend failure, GPU creation, circle and rounded-box drawing, alpha blending, mesh strokes, texture rotation/flip, clipping, font upload, readback, present, resize, coordinate round trips and fullscreen transitions. `sdl3_application_smoke_tests` starts the example game, runs frames and requests normal exit. These demonstrate startup/shutdown behavior, not a dedicated leak-detector audit of all failure branches.

`sdl3_asset_decode_tests` scans the entire assets tree: 83 images, 3 audio files (both decoding policies) and 10 fonts decoded successfully. Current assets contain PNG, WAV, Ogg and TTF; JPEG and MP3 codecs are enabled in the build but have no existing asset fixture. Existing tests cover resource failures, fonts, numeric glyph caches, input routing, scene transitions, sound scheduling and fades. Added audio assertions verify live tracks detach on resource unload.

`runtime-dependencies.log` records dumpbin output for the Release executable, SDL3 DLL and application test. It contains SDL3.dll and system/MSVC libraries, with no SDL2 or SDL2 extension import. `test-runtime-dependencies.log` extends this import check to all 87 Release test executables. `loaded-modules.log` records the running application smoke process: SDL3.dll is loaded and no SDL2 module is present; the process exits with code 0.

## Explained pixel differences

No pre-existing pixel tolerance was widened.

1. SDL3 separates logical presentation from explicit render scale. Hairline sizing, pixel snapping and test sample locations now include presentation scale and letterbox offset.
2. SDL3 software geometry can combine adjacent triangles into rectangles and truncate a scaled one-pixel height to zero. Ring indices submit the two halves separately to retain triangle rasterization; the same mesh and coverage assertions remain.
3. SDL3 readback is limited to the current viewport. The software test flushes, temporarily selects the full output and restores presentation to capture letterbox coordinates correctly.
4. The new GPU alpha assertion allows one byte for UNORM blend rounding, based on the expected source-over result; this does not alter existing tests.

## Manual acceptance

On 2026-09-14, the user confirmed: “确认功能一切正常 可以删除旧文件”. This is user-reported functional acceptance, separate from automated test evidence. No per-device or per-DPI measurements were supplied. The acceptance checklist was:

- Visual/gameplay: inspect existing UI, rounded corners, outlines, text and gameplay. Resize the window and switch fullscreen; compare clipping, texture transparency and animation against expected behavior.
- Pointer/focus: at 100%, 125% and 150% Windows display scaling, click controls, drag sliders, scroll lists and change keyboard focus. Verify letterbox bars do not activate controls.
- Chinese IME: focus a text field, enter and edit Chinese composition, and check candidate-window placement after resizing and moving the window between displays.
- Controller: connect, use and disconnect an actual controller while holding a button or axis. Verify release state and input-device switching.
- Listening: overlap effects, adjust master/group volumes, switch music and scenes, exercise fades and close the application. Check for clipping, unexpected looping, stale audio or abrupt transitions.

The user accepted the functional result; detailed visual, hearing and hardware measurements were not independently recorded by the agent. A GPU screenshot exists at `out/build/sdl3-Debug/tests/sdl3-gpu-verification.png`; it is a focused diagnostic scene, not full gameplay acceptance. Linux, macOS and MinGW are unverified.

Post-acceptance cleanup removed `thirdparty/SDL2`, `imgui_impl_sdl2.{cpp,h}` and `imgui_impl_sdlrenderer2.{cpp,h}`. Historical notes and ignored build/baseline artifacts are retained. Cleanup verification is recorded below. No commit, push or merge is performed automatically.


## Post-cleanup verification (2026-09-14)

- Removed 129 tracked files: the SDL2 dependency snapshot and four unused ImGui backend files. Corrected the remaining SDLRenderer2 name in an error message to SDLRenderer3.
- All three build configurations succeeded after deletion. Full CTest reruns passed: Debug with ImGui 87/87, Release with ImGui 87/87, Release without ImGui 86/86, including actual GPU and application smoke tests.
- Active engine/game/test sources and CMake inputs contain no SDL2 or SDLRenderer2 references. The Release application, SDL3 DLL and all 87 Release tests (89 binaries) have no SDL2 imports.
- Evidence: `out/sdl3-migration/cleanup-build-*.log`, `cleanup-ctest-{debug,release,noimgui}.log` and `cleanup-runtime-dependencies.log`.
- Windows migration acceptance and post-acceptance cleanup are complete. Linux, macOS and MinGW remain unverified.
