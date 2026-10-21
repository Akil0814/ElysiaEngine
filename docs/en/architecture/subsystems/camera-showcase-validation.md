# Camera Showcase Completion Acceptance Record

This document preserves the build, automation, and screenshot results at the time. Dates, environment, and unperformed manual checks are historical facts, not current working-tree validation.

Date: 2026-10-02. Environment: Windows, MSVC Debug, SDL3 software renderer, 1280×720 logical canvas.

## Completed

- Main application and all test targets built. Full CTest **148/148 passed**, final duration 30.14 seconds.
- Four follow strategies, existing multi-target zoom/degradation, regrouping, boundary presets, and zoom-completion-frame regressions passed.
- Three easing curves, three-segment endpoints, ResumeFollow/Hold, pause/resume/cancel, and follow restoration passed. Holding handles retained ownership, notified completion once, and remained pausable/cancelable.
- Full cinematic, direct/blended entry, return to Main, phase pause/skip, replay, actor freezing, and original binding restoration passed. Auto-motion settings restored on return.
- Tab shortcuts, operation-row mouse/keyboard focus/confirm, hidden-page isolation, full return payload, Reuse/Reset/Recreate, entry-failure, and exit cleanup passed.
- Only Main/Cinematic were modified, leaving Auxiliary1 untouched. SceneKey stayed 13. No old scene-type, key-symbol, or filepath references remained.
- Five-language key contracts and `git diff --check` passed. Existing audio changes were preserved; nothing was staged, committed, or pushed in this round.

## Software Rendering Layout Inspection

Screenshots were saved under `out/camera-showcase-qa` and `out/camera-qa`, not committed as project assets.

- Generated all three pages in five languages at 1280×720, plus blend-entry, paused, and path-playback frames in every language. Layout and status text were inspected for each language.
- Generated path-hold, four cinematic phases, boundary, and existing multi-target screenshots and inspected representative frames. World grids, areas, and paths were confined to content; title, status, and footer remained readable.
- Combined node numbers when path endpoints overlapped to prevent text overlap. Action buttons used scrolling rows of height 44; reset and return remained fixed in the footer.

## Human Experience Acceptance Outstanding

- Check keyboard/mouse focus, scrolling operation rows, return, and reset in a real window.
- Experience all four follow responses, distant-target degradation/regrouping, bounds, and zoom.
- Experience cinematic pacing, shake, pause, skip, and transition back to Main.

Automated input regressions and manual software-screenshot inspection are not equivalent to human interaction in a real window.
