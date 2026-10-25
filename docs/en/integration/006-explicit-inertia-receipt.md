# Independent Mass Properties Integration Receipt (006)

Request date: 2026-10-24. Delivery date: 2026-10-25. Corresponds to FlyMe2TheMoon `docs/engine-support-requests/006-explicit-inertia.md`. This receipt lives in ElysiaEngine; the game repository was not modified.

## Delivered Capabilities and Units

`BodyDefinition::mass_properties` uses `MassProperties` with three exclusive alternatives: `DensityMass{}`, `ExplicitMass{mass}`, and `BodyMassData{mass, local_center, rotational_inertia}`. Density computation is the default. Explicit total mass retains proportional inertia scaling. Complete mass data specifies all three properties independently.

Mass uses kg; local center uses EU relative to the body origin; inertia uses kg·EU² about the center of mass. Internally the engine divides length by `units_per_meter` and inertia by its square; games need no Box2D calls. Older data using local-origin inertia must first apply `I_center = I_origin - mass * |local_center|²`; similarly named fields cannot simply be copied.

Use the configuration at creation. At runtime `PhysicsWorld::set_mass_properties(handle, properties)` replaces it, `mass_properties(handle)` reads committed configuration, and `body_state(handle)` reads actual mass, effective inertia, and local/world centers. `PhysicsParticipant` provides matching configuration shortcuts. Passing `DensityMass{}` restores density computation. Queries for pending native creation or invalidated identity return empty.

Only Dynamic bodies accept overrides. Override mass must be finite and positive, center finite, and inertia finite and nonnegative; converted values and reciprocals must be representable. Zero inertia has no torque response. Fixed rotation always has zero effective inertia while retaining the requested configuration. Invalid creation returns an invalid handle; invalid mutation returns false and preserves state.

## Integration and Migration

The old `mass_policy + mass` fields are removed. Use `DensityMass{}` for density computation and `ExplicitMass{original_mass}` for explicit total mass. Retaining density-derived inertia requires complete mass data rather than `ExplicitMass`.

The following code is compiled. The creation configuration targets the centered plank verified for this request. The runtime function preserves currently effective inertia; it does not recover density inertia already scaled by an earlier explicit-mass configuration.

```cpp
#include "engine/physics/contracts/physics_participant.h"
namespace mass_receipt_example {
namespace ph = elysia::physics;
ph::BodyDefinition create_definition() {
    ph::BodyDefinition body;
    body.fixed_rotation = false;
    body.mass_properties = ph::BodyMassData{0.5f, {}, 5466.666667f};
    return body;
}
bool change_mass(ph::PhysicsWorld& world, ph::PhysicsObjectHandle handle) {
    auto state = world.body_state(handle);
    if (!state) return false;
    return world.set_mass_properties(handle,
        ph::BodyMassData{0.5f, state->local_center, state->rotational_inertia});
}
bool restore_density(ph::PhysicsParticipant& object) {
    return object.set_mass_properties(ph::DensityMass{});
}
}
```

A `256 × 8 EU` rectangle at `32 EU/m` and `0.5 kg/m²` has density mass `1 kg` and center inertia approximately `5466.67 kg·EU²`. Creating with `BodyMassData{0.5f, {}, 5466.666667f}` retains that inertia; `ExplicitMass{0.5f}` instead produces approximately `2733.33 kg·EU²`.

| Game level / entity ID | Explicit mass | Current ExplicitMass inertia scaling | Game integration action |
| --- | --- | --- | --- |
| 25 / 0 | 0.5 kg | 0.5 | Use complete mass data to retain density inertia |
| 30 / 1 | 0.5 kg | 0.5 | Use complete mass data to retain density inertia |
| 42 / 0, 1, 6, 9, 10 | 1 kg | 1 | No numerical difference here; configuration fields still need migration |

All seven entities are kind 110. The current game collider is centered, giving zero local center of mass. Offset `Anchor` describes a joint local anchor and does not move the mass center to the pivot. Planks without explicit Mass continue to use `DensityMass{}`. The engine hardcodes no levels, kind numbers, or plank dimensions.

## Execution, Lifecycle, and Backend Correction

Collider replacement, density or mass-contribution changes, and enable changes reapply the configuration. Mutation does not rebuild bodies, colliders, or joints; identities and local joint anchors stay unchanged. Center changes preserve origin pose, angular velocity, and instantaneous origin velocity by adjusting center velocity. Momentum and energy conservation are not promised. Enabled bodies wake; disabled bodies remain disabled.

Callback configurations are copied into the FIFO. True means validated and accepted; queries still return the old configuration until execution. Execution rechecks identity and then-current geometry; removed, Reset, or no-longer-valid requests are skipped. Queue allocation exceptions propagate without committing configuration early.

Bundled Box2D remains 3.1.1. The native mass setter now shares and refreshes extent calculation relative to the effective center for CCD and sleeping. Native IDs stay private. The known disabled-joint-island assertion is outside this fix.

## Automated Validation and Acceptance Boundaries

Full CTest baselines were recorded in all three configurations before implementation. Complete builds and bilingual documentation examples pass in all three configurations. All physics tests pass; the full suite has no new failures.

| Configuration | Build and bilingual examples | Physics tests | Full CTest |
| --- | --- | --- | --- |
| Debug / ImGui ON | Pass | 69/69 | 186/189 |
| Release / ImGui ON | Pass | 69/69 | 186/189 |
| Release / ImGui OFF | Pass | 69/69 | 183/187 |

New tests cover all three policies, independent impulse/torque formulas, nonzero center, origin velocity continuity, fixed rotation, zero inertia, reapplication after collider changes, waking, disabled state, FIFO, deferred creation, removal, Reset, exception cleanup, allocation-point faults, and offset-center CCD. A separate native test verifies extent-cache refresh. Inspector uses owned mass snapshots, including colliderless bodies.

Existing baseline failures are `demo_scene_tests`, `engine_i18n_assets_tests`, and `project_i18n_assets_tests`; no-ImGui additionally fails `application_startup_exception_mid`. This change neither modifies those unrelated behaviors nor weakens their assertions.

Original mass-override behavior is based on the game repository research record; the original binary was not independently re-audited here. FlyMe2TheMoon integration, game-level runs, and actual-window feel acceptance were not performed. Engine formula tests do not prove identical original trajectories. Game integration should check thrust and collision angular response in levels 25 and 30.

## Related Documentation

- [Physics guide](../user-guide/systems/physics.md)
- [Units, commands, and state](../reference/physics/units-and-state.md)
- [Physics API overview](../reference/physics/README.md)

Evidence is saved in repository build output: `out/mass-properties-baseline.md`, `out/mass-sdl3-*-physics.log`, `out/mass-sdl3-*-ctest.log`, and corresponding build logs. Parity checks cover 87 Chinese / 87 English pages; link checks cover 176 Markdown files, both with zero errors. `git diff --check` passes. Debug initially hit capacity errors in old linker PDBs; cleaning generated databases and relinking resolved them without disabling debug information.
