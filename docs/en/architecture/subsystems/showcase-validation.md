# Game Showcase Refactoring Acceptance Record

This document preserves the build, automation, and screenshot results at the time. Dates, environment, and unperformed manual checks are historical facts, not a claim of renewed validation for the current working tree.

Date: 2026-10-01. Environment: Windows, MSVC Debug, SDL3 software renderer, dummy audio driver.

## Completed

- `ElysiaEngine.exe` and all test targets built successfully.
- Full CTest: 146/146 passed. Names, step counts, and thresholds of ten basic, three combat, and three low-tier stress cases were retained.
- Medium/high tiers of three stress cases ran separately, six runs total, all passed. Each tier checked four bodies assertions, five contacts assertions, and twenty-eight tiles assertions over 720 steps.
- Showcase navigation, nested return payloads, cached catalog re-entry, validation Reuse/Reset/Recreate, pause/single-step, multiplayer device restoration, Inspector unregistration, and hidden/rebuilt HUD regressions passed.
- World-UI toggles, bubble reuse, position/projection, and stopping submissions on death/exit passed.
- Generated WAVs loaded. Audio exit canceled owned requests, preserved external ones, and restored volumes/group configuration. Operations were disabled without audio initialization.
- Five-language key contracts passed; scans found no obsolete directory/type/payload references. `git diff --check` passed.

## Software Rendering Layout Inspection

`showcase_render_tests` loaded real project assets at 1280×720 and traversed eight modules, three gameplay scenes, two validation kinds, and five languages. Set `ELYSIA_SHOWCASE_QA_DIR` to save PNGs. These screenshots were in `out/showcase-qa`, not committed as assets.

Inspected catalog, UI pages, input status, camera controls/overlays, animation, effects, audio scrolling, Gameplay catalog, HUD, and validation layouts. Camera overlays were clipped to content rather than covering titles/controls. These checks do not constitute real-window interaction acceptance.

## Manual Acceptance Outstanding

- Check five-language long text, scrolling, keyboard/mouse focus, and return actions in a real 1280×720 window.
- Experience collision/platform/top-down combat, camera strategies, zoom, bounds, and control switching.
- Listen to immediate/delayed sounds, looping/stopping, group limits, two music transitions, and fades.

Automated audio tests used dummy output and checked requests/cleanup only; no listening acceptance was performed.
