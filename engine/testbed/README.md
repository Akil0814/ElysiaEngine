# Engine Testbed

`engine/testbed` owns interactive, runtime engine experiments that may depend on
multiple engine subsystems. It is a leaf module: production engine subsystems
must not depend on Testbed code.

- Put each camera, physics, effects, or other subsystem showcase in its own scene.
- Put Testbed-only runtime `GameObject` types in `engine/testbed/objects/` when
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
Press `Escape` to return to the caller.
