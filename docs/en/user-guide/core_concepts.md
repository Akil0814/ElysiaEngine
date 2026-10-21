# Essential Game Development Concepts

This document introduces the concepts needed before developing with Elysia Engine: scenes and objects, ownership and lifetime, updates, `object` states, and coordinates. A health `potion` example connects them at the end; it illustrates responsibilities and steps, not a complete runnable game.

> **Reading note**: this document focuses on game-layer behavior contracts rather than every engine implementation detail.

## 1. Core Code to Read First

| File | Role |
| --- | --- |
| [scene.h](../../../engine/scene/scene.h) | `Scene` extension points and `object` ownership |
| [scene_object.h](../../../engine/core/scene_object.h) | Common scene `object` base and basic states |
| [game_object.h](../../../engine/core/game_object.h) | Base implementation for world objects |
| [render_command.h](../../../engine/core/render/render_command.h) | Drawing command data structures |

## 2. Responsibilities of Scenes, Game Objects, and UI

The core relationship can be simplified to:

```text
Scene (organizes runtime content, owns objects)
└── SceneObject (common object base)
    ├── GameObject (world object)
    └── UiElement (UI element)
```

This is a **combined organization and inheritance diagram**: `Scene` owns objects and does not inherit `SceneObject`; `GameObject` and `UiElement` do inherit it.

### 2.1 SceneObject: Common Base

`SceneObject` provides basic active, visible, and destroyed states plus reset/pause-related interfaces. `GameObject` and `UiElement` separately provide drawing command submission. Only method names are shown below; parameters and declarations are omitted:

```cpp
// GameObject: submit world drawing commands.
submit_render_commands(...);
// UiElement: submit UI drawing commands.
submit_ui_render_commands(...);
```

Drawable objects assemble their own commands; the scene collects and executes them together. **Submitting commands does not draw immediately**, and an `object` need not submit any drawing content.

Inheriting `SceneObject` does not automatically enable frame updates, input, or physics collision. Explicitly implement and integrate those capabilities under each system's rules.

### 2.2 Scene: Runtime Content Organizer

`Scene` organizes an independent part of runtime content, for example:

```text
MainMenuScene
SpecialEventsScene
BattleScene
```

It owns scene objects, coordinates input, updates, drawing command collection, and cleanup, and manages entry, exit, and reset. Scenes should handle **`object` organization and overall flow** without implementing every `object`'s behavior.

For example, `BattleScene` can organize the player, enemies, items, background, and battle UI. Player/control logic should handle movement; item/interaction systems should handle pickup. Do not put every behavior into `BattleScene`.

Each scene owns an independent `object` collection. Conceptually, ownership looks like:

```cpp
std::vector<std::unique_ptr<elysia::core::SceneObject>> objects;
```

This ownership sketch omits separate world-`object` and UI-root storage and does not represent actual container layout. Scenes accept only objects derived from `GameObject` or `UiElement`, not arbitrary `SceneObject` subclasses.

### 2.3 GameObject: World Objects

`GameObject` represents actors, items, projectiles, or decorations in the world. It supplies world rectangles, drawing layers, and `order` within layers.

Inheriting `GameObject` **does not automatically provide**:

- Movement or input response;
- Rigid bodies, colliders, or pickup detection;
- Frame or fixed-step updates.

Implement these as needed and integrate their update paths or subsystems. Static decorations and players may both be `GameObject` without sharing behavior.

### 2.4 Drawing Layers and Occlusion Order

Constructing a `GameObject` requires a `DepthLayer`; optional `order` sets `order` within that layer (default `0`, queried with `order_in_layer()`). These determine world drawing `order`.

Current layers draw back to front as follows:

```text
Background → Terrain → EffectBack → Item → Character → EffectFront → Foreground
```

Later content generally appears over earlier content, subject to opacity and other drawing settings. `Count` is an enum layer-count marker, not a usable layer.

Names express drawing purpose: backgrounds use `Background`, items use `Item`, effects in front of actors use `EffectFront`. They grant no behavior: `Character` grants no actor capabilities, and `Terrain` creates no collisions.

**Compare layer first, then `order` within it.** Larger `order_in_layer()` draws later within one layer, without crossing layers. Even an `Item` with a large `order` is drawn before `Character`.

For example, a `potion` can select its item layer in its constructor:

```cpp
HealthPotion()
    : GameObject(elysia::core::DepthLayer::Item, 0)
{
}
```

UI has independent ordering. Scenes currently draw world objects before UI; raising a `GameObject` layer/`order` cannot place it over UI.

### 2.5 UiElement: Interface Elements

`UiElement` represents buttons, HUD health bars, inventory panels, and other interface elements. Add it directly to a scene or a UI container. Containers own their children and manage layout, interaction, and cleanup. See [world-space Gameplay UI](gameplay/world-ui.md) for names/health bars above actors that move with the world camera.

`UiElement` and `GameObject` both inherit `SceneObject`, but **a shared base does not imply shared coordinates or updates**:

| Property | `GameObject` | `UiElement` |
| --- | --- | --- |
| Purpose | World actors, enemies, items | Buttons, health bars, inventory |
| Coordinates | World coordinates | UI coordinates and container layout |
| Camera | Drawing uses the corresponding world camera projection | Does not follow ordinary world camera projection |

A dropped `HealthPotion` is a `GameObject`; its inventory icon is UI. Independent game logic should maintain inventory **data**, with UI displaying it. Do not use a soon-to-be-destroyed world item pointer as inventory data.

## 3. Object Creation, Membership, and Ownership

Objects **successfully added to a scene are owned by it**. UI children added to containers are owned by those containers. Never let two owners manage the same `object`.

`Scene` creation/addition interfaces have these forms; template declarations and constraints are omitted, so these are not complete declarations to copy:

```cpp
T* create_and_add_object(Args&&... args);
T* add_object(std::unique_ptr<T> object);
```

### 3.1 Letting the Scene Create Objects

Assume `HealthPotion` has a no-argument constructor:

```cpp
HealthPotion* potion = scene.create_and_add_object<HealthPotion>();
```

Specify the `object` type for `create_and_add_object`; pass its constructor arguments in parentheses. The interface perfectly forwards them to the constructor. Required arguments depend on the class definition.

### 3.2 Transferring Ownership of Existing Objects

```cpp
auto potion = std::make_unique<HealthPotion>();
HealthPotion* raw = scene.add_object(std::move(potion));
```

`std::move()` transfers `unique_ptr` ownership. After successful insertion, the scene manages the `object`; the original `unique_ptr` no longer owns it.

Creation or registration failure may return null or throw. Check the result; do not assume failure returns ownership to the original pointer. Synchronous insertion is allowed only in phases permitting collection mutation; see the [scene phase table](scene/scene.md#common-operations-and-extension-points).

### 3.3 Returned Raw Pointers Do Not Own Objects

Both interfaces return `T*` for access only; it is not responsible for release:

```cpp
HealthPotion* potion = scene.create_and_add_object<HealthPotion>();

// Incorrect: the scene already owns the object.
// delete potion;

// Incorrect: never create a second owner for the same object.
// std::unique_ptr<HealthPotion> another_owner(potion);
```

**Scenes manage `object` lifetime; returned raw pointers provide access only.**

## 4. Object Lifetime and Destruction

### 4.1 `destroy()` Only Marks Destruction

For every `SceneObject`, `destroy()` marks destruction without immediately freeing memory:

```cpp
potion->destroy();
```

Direct scene-owned objects are removed and released at cleanup after updates (`on_after_update`), exit callbacks, or reset callbacks. UI containers clean up their own children.

`Scene` objects marked in `on_exit()` are reclaimed within that exit, without waiting for reentry. Unmarked objects can remain in cached scenes for `Reuse`. `Reset` calls scene `on_reset()` without automatically clearing/resetting every game `object`.

Treat a marked `object` as leaving use; do not schedule new operations merely because it temporarily remains in memory.

### 4.2 Raw Pointers Do Not Automatically Become Null

```cpp
HealthPotion* potion = scene.create_and_add_object<HealthPotion>();
if (potion)
    potion->destroy();

// Release at subsequent scene cleanup.
// After release, potion does not automatically become nullptr.
```

After actual release, saved `potion` may become a **dangling pointer**. Non-null does not prove validity:

```cpp
if (potion != nullptr)
{
    // This does not prove the object is still alive.
}
```

Even this cannot detect an already freed `object`:

```cpp
if (potion && !potion->is_destroyed())
{
    // Calling is_destroyed() itself requires a valid potion.
}
```

Distinguish:

| Situation | Meaning |
| --- | --- |
| `nullptr` | Null pointer |
| Valid pointer | Refers to a live `object` |
| Marked destroyed | Object may not yet be freed, but is in destruction flow |
| Dangling pointer | Object was freed; the old pointer may remain non-null |

### 4.3 Deferred Cleanup Timeline

```text
Call potion->destroy()
        ↓
Object marked for destruction (no immediate delete at the call)
        ↓
A scene cleanup point actually executes
        ↓
Scene removes and releases marked objects after update or exit/reset callbacks
        ↓
Externally retained potion pointers may dangle
```

Deferred cleanup is not an absolute guarantee of survival until the next update.

### 4.4 References and Callbacks Across Frames

Keeping non-owning `Player*`, `Enemy*`, or `HealthPotion*` is not inherently wrong, but establish:

- When can the referenced `object` be destroyed?
- Can this `object` or callback outlive it?
- Is the reference valid after scene switching/reset?
- Who clears it at the appropriate time?

In particular, never let a longer-lived event callback capture an already freed `object` pointer. Without a dedicated safe reference mechanism, developers must maintain cross-frame reference validity themselves.

`std::unique_ptr` solves **ownership**; it does not invalidate other raw pointers or turn them into `nullptr`.

## 5. Scene Caching, Entry, and Reentry

Scenes may be cached. Exit does not imply destruction, and reentry does not necessarily reconstruct objects. Distinguish retaining state, resetting state, and rebuilding an instance.

Game scenes must implement three extension points. These simplified signatures use `ScenePayload` from `elysia::scene`:

```cpp
on_enter(const ScenePayload& payload);
on_exit();
on_reset();
```

`on_enter()` and `on_exit()` handle entry/exit. For `Reset` routes, the engine resets framework state before `on_reset()` restores game state. Games do not directly call these lifecycle functions. Object `reset()` is separate; scenes do not automatically call it for every game `object`.

`Scene` reentry modes are:

```cpp
enum class SceneReloadMode
{
    Reuse,
    Reset,
    Recreate
};
```

Their design intent is:

| Mode | Meaning |
| --- | --- |
| `Reuse` | `Reuse` an existing scene |
| `Reset` | Run reset on an existing scene |
| `Recreate` | Create a new scene instance |

On reentry, do not assume item counts reset, enemies clear, event bindings detach, or old pointers stay valid. Inspect exit/reset logic before deciding what to retain or clear.

Returning from pause may preserve level state; restarting may respawn enemies, reset player position, and clear dropped items. Keep these intentions distinct.

## 6. When Code Runs

Start with this **conceptual flow** for a normal frame:

```text
Input handling
   ↓
Scene update
  ├─ Ordinary update
  ├─ Fixed steps as needed from accumulated time
  ├─ Other engine-defined phases
  └─ End of update: clean up marked objects
   ↓
Rendering
```

This is conceptual, not the complete internal call `order`.

### 6.1 Ordinary Update: update

Ordinary game objects must implement `Updatable` and join the scene to register for frame updates. Inheriting `GameObject` or writing a same-named method alone does not cause invocation. Execution also depends on `object` state and pause rules.

`delta` is elapsed time in seconds. At a speed of 100 world units per second, hardcoding movement per frame creates frame-rate dependence:

```cpp
// Incorrect: fixed distance per frame makes speed frame-rate dependent.
position.x += 100.0f;

// Basic time calculation: advance by elapsed time.
position.x += 100.0f * delta;
```

This illustrates time calculation only. Physics-driven objects must follow physics state mutation rules instead of arbitrarily rewriting position.

update generally suits ordinary game behavior, timers, cooldowns, non-physics animation state, and presentation logic advanced each frame. For example:

```cpp
// Private member: double _cooldown = 0.0; // Remaining cooldown in seconds.
// Advance in update(double delta):
_cooldown = std::max(0.0, _cooldown - delta);
```

Verify the game-time, pause, and time-scale rules governing `delta`.

### 6.2 Fixed-Update Extension Points

Fixed updates use a fixed simulation time step, without one-to-one correspondence with rendered frames: a frame may contain zero, one, or multiple fixed updates.

Typical uses:

- Physics simulation;
- Behavior tightly coupled to fixed-step physics state;
- Rules requiring stable simulation steps or specific execution `order`.

An ordinary `Scene` with fixed stepping enabled uses `on_fixed_update(tick, delta)`; `GameplayScene` uses `on_game_fixed_update(tick, delta)`; a `PhysicsStepParticipant` registered with the physics world uses `fixed_update(double)`. Merely adding a same-named method to an ordinary `object` does not register it. See [physics guide](systems/physics.md).

Do not advance the same physics state in both frame and fixed updates; this causes double movement or desynchronization.

Simplified choices:

| Need | Typical choice |
| --- | --- |
| Ordinary timers and cooldowns | update |
| Non-physics presentation state | update |
| Physics simulation | Corresponding fixed-step physics system |
| Rules synchronized with physics | `Scene` fixed-step hook or `PhysicsStepParticipant::fixed_update` |

### 6.3 Rendering Only Presents State

World `submit_render_commands(...)` and UI `submit_ui_render_commands(...)` assemble drawing commands. They must not grant rewards, deduct health, consume input, advance timers, or modify game-rule state as side effects. Otherwise game logic depends on whether rendering runs and its frame rate.

Basic principle:

```text
Update: calculate and modify game state
Render: submit drawing commands from current state
```

## 7. Commonly Confused Object States and Pause

### 7.1 Visibility

Set with `set_visible()`, query with `is_visible()`. Visibility controls drawing, not updates, collision, timers, or input. Hiding an item neither picks it up nor destroys it.

### 7.2 Activity

Set with `set_active()`, query with `is_active()`. Activity filters relevant updates/input, not visibility. For ordinary `GameObject`, drawing checks visibility/destruction and does not use activity directly as its drawing switch.

### 7.3 Destruction State

Mark with `destroy()`, query with `is_destroyed()`. This means an `object` is leaving use, distinct from temporary hiding/deactivation. Temporary invisibility need not require destruction; a consumed one-use item can enter destruction flow.

### 7.4 Pause State

Paused scenes skip objects not allowed to update during pause and stop scene physics. Objects permitted during pause still run. Input has separate pause rules.

`Scene` `on_before_update()` and `on_after_update()` still run. Explicitly check `is_paused()` when advancing business logic that should stop during pause in these hooks. Global time also continues.

Pause menus still need button input and UI animation. **`Scene` pause does not imply the entire application is motionless.**

## 8. World Coordinates, UI Coordinates, and Render Positions

### 8.1 Do Not Mix Coordinate Spaces

World coordinates describe actors, enemies, and items. Distinguish window coordinates from logical render coordinates: window scaling or logical mapping can break one-to-one correspondence. Engine input converts mouse window coordinates into logical render coordinates. `Camera::world_to_screen()` returns logical render coordinates within the viewport, not physical window pixels. UI positions may also depend on container-local coordinates and layout.

For example, a screen-coordinate mouse position cannot be compared directly with a world-coordinate `potion` rectangle:

```cpp
// If mouse_position uses screen coordinates and potion_rect world coordinates,
// they cannot be compared directly:
// potion_rect.contains(mouse_position);
```

Convert through the relevant camera or coordinate mechanism into **the same space** before comparing.

Place this fragment in a scene member function with a camera enabled; `object` is a valid world `object`:

```cpp
// Screen position corresponding to the logical center
const elysia::core::Vector2 screen_center = camera().world_to_screen(object.center());

// Screen region corresponding to the object's displayed rectangle
const elysia::core::Rect screen_rect = camera().world_to_screen(object.render_rect());
```

### 8.2 `world_rect()` and `render_rect()`

`world_rect()` is the `object`'s world rectangle; `render_rect()` may include display offsets from physics interpolation.

When the corresponding interpolation mechanism works, the engine computes display offsets automatically; **manual updates are unnecessary**.

**Game rules use logical state; drawing uses presentation state.** Never write interpolated display positions back to business or physics state.

## 9. Conceptual Example: A Dropped `HealthPotion`

Suppose a player sees a health `potion`, meets pickup conditions, restores health, and removes the `potion` from the world. An inventory system could instead store the item.

These fragments omit headers, drawing resources, player types, and pickup detection, showing only responsibility divisions. See [game objects and rendering](scene/game-objects.md) for compilable `object` types.

### Step One: Define the Object

```cpp
class HealthPotion : public elysia::core::GameObject
{
public:
    HealthPotion()
        : GameObject(elysia::core::DepthLayer::Item, 0)
    {
    }

    // Implement drawing and pickup behavior as needed.
};
```

It inherits `GameObject` because it is a world item. Pickup colliders, input interaction, and frame updates **do not appear automatically**; integrate the chosen capabilities explicitly. A stationary item awaiting interaction may need no frame update.

### Step Two: Add It to the Scene

Using the no-argument constructor above:

```cpp
HealthPotion* potion =
    scene.create_and_add_object<HealthPotion>();
```

After checking `potion` for null, set position/size with `set_world_rect()` and implement drawing below. The scene must explicitly enable a camera to display world objects. After insertion the scene owns it; the returned pointer is access only.

### Step Three: Implement Drawing and Interaction as Needed

The `potion` can submit its drawing commands. Game code chooses pickup detection, such as trigger regions, an interaction system, or `object` queries.

For floating motion or timed disappearance, implement/register the corresponding update capability as needed. Do not unconditionally enable frame updates for every item.

### Step Four: Handle Pickup

```text
Confirm pickup conditions
        ↓
Restore health or update inventory data
        ↓
Prevent duplicate effects from the same potion
        ↓
Mark destruction: potion->destroy()
```

Pickup logic should prevent duplicate processing; after granting the effect, mark the `potion` for scene cleanup.

### Step Five: Release and Reference Cleanup

Following section 4's lifetime rules, check references in interaction targets, selection, and deferred callbacks, and detach invalid associations before release.

### Step Six: Update Inventory UI (If Present)

Separate these responsibilities:

```text
HealthPotion     World item instance
Inventory        Game data such as item types and counts
InventoryPanel   Inventory display
InventorySlot    Individual item display
```

Update inventory data first, then mark the world `potion` destroyed; UI reads inventory data to refresh. Do not keep `HealthPotion*` as a long-term inventory record, which can dangle after scene cleanup.

## 10. Common Misconceptions

| Misconception | Correct understanding |
| --- | --- |
| Inheriting `GameObject` enables updates | Implement and register update capabilities |
| Inheriting `GameObject` enables collision | Configure and integrate physics or interaction |
| `destroy()` releases immediately | Marks only; release occurs at a subsequent executed cleanup point |
| Non-null raw pointers are valid | Freed objects can leave non-null dangling pointers |
| Hiding stops all behavior | Visibility mainly controls drawing |
| Inactive means invisible | Activity and visibility are distinct |
| Pause stops the entire program | Some updates and UI input can continue |
| Each frame has exactly one fixed update | It can have zero, one, or multiple |
| All timers need fixed updates | Ordinary timers can generally accumulate `delta` |
| Rendering can also modify game rules | Separate state updates from presentation |
| Mouse coordinates compare directly with world rectangles | Unify coordinate spaces |
| `render_rect()` is the physics position | It may contain display-only interpolation offsets |
| Leaving a scene reinitializes it | Scenes may be cached, reused, or reset |

## 11. Before-Development Checklist

Before adding objects or features, confirm:

- Does the `object` belong to the world or UI?
- Which `Scene` or UI container owns it?
- Must update, input, or physics capabilities be explicitly integrated?
- Should the logic run in ordinary update, fixed update, or another phase?
- When is it destroyed, and who actually cleans it up?
- Are there raw pointers across frames or callbacks that may run after the `object`?
- Do all position comparisons use the same coordinate space?

## 12. Summary

`Scene` organizes and owns objects; `GameObject` and `UiElement` represent the world and UI respectively. Integrate update, input, and physics explicitly as needed rather than inferring them from inheritance.

Scenes/containers manage owned lifetimes; users must still maintain cross-frame references and callback validity.

Ordinary and fixed updates serve different time models. Rendering presents state rather than advancing rules. Distinguish world, screen, and UI coordinates as well as logical positions and interpolated display positions.
