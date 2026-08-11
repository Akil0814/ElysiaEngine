# Demo Gallery and Project Testbed

The project-owned `DemoGalleryScene` is the single runtime entry for showcases.
It lives under `game/scene/demo/` together with `AnimationPreviewScene` and the
shared `DemoScenePayload`. `game/testbed` continues to own the UI workbench and
engine-feature experiments that may depend on multiple engine subsystems.
None of these files are compiled into or registered by `engine_lib`.

The main menu links only to Demo Gallery, Settings, and Exit. Demo Gallery then
routes to Physics & Combat, Animation Preview, UI Component Gallery, Engine
Feature Lab, Elysia Realm, and the guarded Application Failure test. Every demo
receives a complete return route through `DemoScenePayload`, so Escape restores
the caller key, payload, and reload mode. The failure test is a danger action and
requires confirmation before it enters the terminating engine failure flow.

- Put each camera, physics, effects, or other subsystem showcase in its own scene
  and register its entry through Demo Gallery.
- Put Testbed-only runtime `GameObject` types in `game/testbed/objects/` when
  they are introduced.
- Keep automated unit and integration tests under `tests/`; that directory is
  not runtime Testbed code.
- Do not place production startup, settings, or failure scenes here; they remain
  under `engine/builtin/scenes/`.

## Engine Feature Test

The animation comparison keeps the left sprite unmodified and applies a coverage
mask color overlay to the right sprite. Press `Space` to cycle through no
overlay, white, blue, purple, and gray. The world-space `EngineCharacter` at the
center uses the built-in idle and move animations; move it with WASD or the arrow
keys. Its green AABB is submitted through the `PhysicsCollider` debug category.
The control window contains a horizontally scrollable set of `Damage`, `Critical`, `Heal`, `Percent`,
`Fraction`, and `Decimal` buttons that spawn representative floating-number
effects above the character. Press `Escape` to return to the caller.
