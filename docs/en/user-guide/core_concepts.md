# Essential Game Development Concepts

This document introduces the fundamental concepts you should understand before developing games with **Elysia Engine**: scenes and objects, ownership and lifetimes, update mechanisms, object states, coordinate systems, and a complete example of implementing an in-game item.

> **Reading note:** This document focuses on the behavioral contracts that game-layer code should follow, rather than every detail of the engine's internal implementation.

## 1. Core Source Files to Read First

| File | Purpose |
| --- | --- |
| `scene_object.h` | Common base class and basic states for scene objects |
| `game_object.h` | Base implementation for objects in the game world |
| `render_command.h` | Data structures for rendering commands |

## 2. Responsibilities of Scenes, Game Objects, and UI

The core relationships can be simplified as follows:

```text
Scene (organizes runtime content and owns objects)
└── SceneObject (common object base class)
    ├── GameObject (game-world object)
    └── UiElement (UI element)
```

This diagram **combines organizational and inheritance relationships**: `Scene` owns objects but does not inherit from `SceneObject`; `GameObject` and `UiElement` both inherit from `SceneObject`.

### 2.1 SceneObject: Common Base Class

`SceneObject` provides basic states, such as active, visible, and destroyed, as well as reset and pause-related interfaces. Rendering-command submission is provided separately by `GameObject` and `UiElement`:

```cpp
// GameObject: submit rendering commands for world objects.
submit_render_commands(...);
// UiElement: submit UI rendering commands.
submit_ui_render_commands(...);
```

Objects that need to be drawn assemble their own rendering commands, which the scene system then collects and executes together. **Submitting commands does not draw anything immediately.** An object is not required to submit any rendering commands at all.

Inheriting from `SceneObject` does not automatically provide per-frame updates, input handling, or physics collisions. These capabilities must be explicitly implemented and integrated according to the rules of the relevant systems.

### 2.2 Scene: Organizer of Runtime Content

`Scene` organizes an independent segment of runtime content, for example:

```cpp
MainMenuScene
SpecialEventsScene
BattleScene
```

It owns scene objects, coordinates input, updates, rendering-command collection, and object cleanup, and manages scene entry, exit, and reset procedures. A scene should focus on **organizing objects and coordinating the overall flow**; it does not need to implement every object's specific behavior.

For example, a `BattleScene` might organize players, enemies, items, backgrounds, and combat UI. Player movement should be handled by the player or controller-related logic, and item pickup by the item or interaction system, rather than putting every behavior into `BattleScene`.

Each scene has its own collection of objects. From an ownership perspective, it can be understood conceptually as:

```cpp
std::vector<std::unique_ptr<elysia::core::SceneObject>> objects;
```

**This is only a conceptual example; it does not mean the scene internally uses `std::vector`.**

### 2.3 GameObject: Game-World Object

`GameObject` represents characters, items, projectiles, decorations, and other objects in the game world. It provides basic capabilities such as a world-space rectangle, rendering layer, and ordering within that layer.

However, inheriting from `GameObject` **does not automatically provide**:

- Movement or input handling;
- Physics bodies, colliders, or pickup detection;
- Per-frame or fixed-step updates.

These capabilities must be implemented as needed and integrated with the corresponding update mechanism or subsystem. A static decoration and a player character may both be `GameObject`s without needing the same behaviors.

### 2.4 Rendering Layers and Draw Order

When creating a `GameObject`, you must specify a `DepthLayer` in its constructor. You may also specify an ordering value within the layer, `order_in_layer` (default: `0`). Together, these determine the draw order of world objects.

The current layers are drawn from back to front in this order:

```text
Background → Terrain → EffectBack → Item → Character → EffectFront → Foreground
```

Content drawn later generally appears over content drawn earlier, although actual occlusion also depends on transparency and other rendering settings. `Count` is only an enum marker indicating the number of layers; it is not a usable rendering layer.

Layer names express an object's intended rendering role. For example, backgrounds use `Background`, items use `Item`, and effects in front of characters use `EffectFront`. Layers do not automatically grant behavior: selecting `Character` does not give an object character capabilities, and selecting `Terrain` does not automatically create collisions.

**Compare the layer first, then the order within that layer.** Within the same layer, a larger `order_in_layer` is drawn later. This value cannot override the layer ordering. Even an `Item` object with a very large order value cannot be moved in front of objects in the `Character` layer: `Character` is still drawn after `Item`.

For example, a potion can select the item layer in its constructor:

```cpp
HealthPotion()
    : GameObject(elysia::core::DepthLayer::Item, 0)
{
}
```

UI uses a separate ordering system. The current scene draws world objects first and UI afterward. Raising a `GameObject`'s layer or order within its layer cannot make it draw over the UI.

### 2.5 UiElement: User Interface Element

`UiElement` represents UI elements such as buttons, health bars, and inventory panels. It can be added directly to a scene or placed inside a UI container. A child added to a container is owned by that container and follows the container's layout, interaction, and cleanup procedures.

`UiElement` and `GameObject` both inherit from `SceneObject`, but **sharing a base class does not mean sharing a coordinate system or update mechanism**:

| Property | `GameObject` | `UiElement` |
| --- | --- | --- |
| Purpose | Characters, enemies, items, etc. in the world | Buttons, health bars, inventories, etc. |
| Coordinates | World coordinates | UI coordinates and container layout |
| Camera | Drawn through the appropriate world-camera projection | Does not follow the regular world-camera projection |

For example, a `HealthPotion` dropped on the map is a `GameObject`, while the potion icon displayed in an inventory belongs to the UI. Inventory **data** should preferably be maintained by independent game logic, with the UI responsible for displaying it. Do not use a pointer to a world item that is about to be destroyed as the inventory data itself.

## 3. Object Creation, Ownership, and Responsibility

All objects **successfully added to a scene are owned by that scene**. If a UI child is added to a container, the corresponding container owns it instead. Do not let two owners manage the same object simultaneously.

The scene provides object-creation and addition interfaces in the following forms:

```cpp
T* create_and_add_object(Args&&... args);
T* add_object(std::unique_ptr<T> object);
```

### 3.1 Let the Scene Create an Object

Assuming `HealthPotion` provides a default constructor:

```cpp
HealthPotion* potion = scene.create_and_add_object<HealthPotion>();
```

When using `create_and_add_object`, specify the object type. Supply the arguments needed to construct the object inside the parentheses; the interface perfectly forwards them to the constructor. The exact arguments depend on the class definition.

### 3.2 Transfer Ownership of an Existing Object

```cpp
auto potion = std::make_unique<HealthPotion>();
HealthPotion* raw = scene.add_object(std::move(potion));
```

`std::move()` is used to transfer ownership of the `unique_ptr`. Once the object has been successfully added, it is managed by the scene; the original `unique_ptr` no longer owns it.

### 3.3 A Returned Raw Pointer Does Not Convey Ownership

Both interfaces return a `T*`, but this is only a way to access the object. It is not responsible for releasing it:

```cpp
HealthPotion* potion = scene.create_and_add_object<HealthPotion>();

// Wrong: the scene already owns this object.
// delete potion;

// Wrong: do not create a second owner for the same object.
// std::unique_ptr<HealthPotion> another_owner(potion);
```

**The scene manages the object's lifetime; the returned raw pointer only provides access.**

## 4. Object Lifetime and Destruction

### 4.1 `destroy()` Only Marks an Object for Destruction

Calling `destroy()` on any `SceneObject` only marks it for destruction. It does not immediately free its memory at the call site:

```cpp
potion->destroy();
```

Objects owned directly by a scene are removed and freed when the cleanup point at the end of `Scene::on_update()` actually runs after they are marked. Child nodes in UI containers are cleaned up by their respective containers.

If an object is marked for destruction in `on_exit()` and the old scene no longer updates, its regular cleanup also stops. Objects still owned by that scene will be released when the scene is entered again and cleanup runs, or when the entire scene is destroyed. Updates to other scenes do not perform cleanup on its behalf.

Once an object is marked, treat it as about to leave service. Do not rely on the fact that it temporarily remains in memory to schedule new gameplay operations on it.

### 4.2 Raw Pointers Do Not Automatically Become Null

```cpp
HealthPotion* potion = scene.create_and_add_object<HealthPotion>();
potion->destroy();

// The object is freed when scene cleanup subsequently runs.
// potion does not automatically become nullptr after the object is freed.
```

After the object is actually released, the saved `potion` pointer may become a **dangling pointer**. Checking for a non-null value does not prove that the object is still valid:

```cpp
if (potion != nullptr) {
    // This does not establish that the object is still alive.
}
```

Even the following cannot detect an object that has already been freed:

```cpp
if (potion && !potion->is_destroyed()) {
    // Calling is_destroyed() itself requires potion to point to a valid object.
}
```

Distinguish between these cases:

| State | Meaning |
| --- | --- |
| `nullptr` | The pointer is null |
| Valid pointer | Points to an object that is still alive |
| Marked for destruction | The object may not have been freed yet, but destruction has been requested |
| Dangling pointer | The object has been freed, but the old pointer may still be non-null |

### 4.3 Deferred Cleanup Timeline

```text
Call potion->destroy()
        ↓
Object is marked for destruction (no immediate delete at the call site)
        ↓
The scene's cleanup point actually runs
        ↓
End of scene update: remove and free objects marked for destruction
        ↓
External pointers to potion may now be dangling
```

Do not interpret deferred cleanup as an absolute guarantee that an object will remain alive until the next update.

### 4.4 References Across Frames and Callbacks

Keeping non-owning pointers such as `Player*`, `Enemy*`, or `HealthPotion*` is not inherently wrong, but you must establish:

- When might the referenced object be destroyed?
- Could the current object or callback outlive the referenced object?
- Will the reference still be valid after a scene switch or reset?
- Who is responsible for clearing the reference at the appropriate time?

In particular, do not let a longer-lived event callback capture a pointer to an object that has already been freed. Until a dedicated safe-reference mechanism exists, game developers must maintain the validity of cross-frame references themselves.

`std::unique_ptr` solves the **ownership** problem. It does not automatically invalidate other raw pointers or set them to `nullptr`.

## 5. Scene Caching, Entry, and Re-entry

Scenes may be cached. Leaving a scene does not necessarily destroy its instance, and entering it again does not necessarily reconstruct all its objects. You must therefore distinguish between preserving state, resetting state, and recreating an instance.

Relevant lifecycle interfaces:

```cpp
on_enter(const ScenePayload& payload);
on_exit();
reset();
```

`on_enter()` and `on_exit()` correspond to entering and leaving a scene, respectively. `reset()` executes the reset logic that has been implemented; **it does not mean calling the constructor again**.

Scene re-entry modes are defined as follows:

```cpp
enum class SceneReloadMode
{
    Reuse,
    Reset,
    Recreate
};
```

Their intended meanings are:

| Mode | Meaning |
| --- | --- |
| `Reuse` | Reuse an existing scene |
| `Reset` | Run the reset procedure on an existing scene |
| `Recreate` | Create a new scene instance |

When re-entering a scene, do not assume the item count has returned to zero, enemies have been cleared, event bindings have been removed, or old pointers remain valid. Check the scene's actual exit and reset logic before deciding which data to preserve or clear.

For example, returning to a level from a pause menu might require preserving state, while restarting a level might require respawning enemies, resetting the player's position, and clearing the current run's dropped items. These are different operations.

## 6. When Does Game Code Run?

Use the following **conceptual flow** to understand a normal frame:

```text
Input processing
   ↓
Scene update
  ├─ Regular updates
  ├─ Fixed-step updates as needed based on accumulated time
  ├─ Other engine-defined procedures
  └─ End of update: clean up objects marked for destruction
   ↓
Rendering
```

This is only a conceptual overview, not a complete account of the engine's internal call order.

### 6.1 Regular Updates: `update`

A regular game object must implement `Updatable` and be added to a scene to be registered with the corresponding per-frame update process. Merely inheriting from `GameObject` or writing a method with the same name does not cause the engine to call it automatically. Whether it runs during the current frame also depends on the object's state and the pause rules.

In regular updates, `delta` is the elapsed time in seconds. If an object should move at 100 world units per second, adding a fixed distance each frame creates frame-rate-dependent movement:

```cpp
// Wrong approach: moving a fixed distance per frame makes speed depend on frame rate.
position.x += 100.0f;

// Basic time-based approach: advance according to elapsed time.
position.x += 100.0f * delta;
```

This only illustrates time-based calculations. If an object is driven by the physics system, follow that module's rules for changing its state instead of modifying its position arbitrarily.

`update` is usually suitable for general gameplay behavior, ordinary timers, ability cooldowns, animation states not driven by physics, and presentation logic advanced according to frame intervals. For example:

```cpp
// Member variable: double cooldown = 0.0; // Remaining cooldown in seconds.
// Update inside update(double delta):
cooldown = std::max(0.0, cooldown - delta);
```

In actual use, check how `delta` relates to game time, pausing, and time scaling.

### 6.2 Fixed Updates: `fixed_update`

Fixed updates use a fixed simulation time step. They do not correspond one-to-one with rendered frames: a rendered frame may execute zero, one, or multiple fixed updates.

Fixed updates are usually appropriate for:

- Physics simulation;
- Behavior closely coupled to fixed-step physics state;
- Gameplay rules requiring a stable simulation step or a particular update order.

Do not advance the same physics state separately in both `update` and `fixed_update`, as that can cause duplicate movement or inconsistent state. Physics-driven objects should follow the update conventions of their physics system.

A simplified guide:

| Requirement | Usually use |
| --- | --- |
| Ordinary timers and typical ability cooldowns | `update` |
| General presentation state not driven by physics | `update` |
| Physics simulation | The corresponding fixed-step physics system |
| Gameplay rules synchronized with physics simulation | `fixed_update` or the stage prescribed by the physics module |

### 6.3 Rendering Only Presents State

`GameObject::submit_render_commands(...)` and `UiElement::submit_ui_render_commands(...)` assemble rendering commands. They should not also grant rewards, deduct health, consume input, advance timers, or modify state that affects gameplay rules. Otherwise, gameplay behavior would depend on whether rendering occurs and on the rendering frame rate.

Basic principle:

```text
Update phase: compute and modify game state
Render phase: submit rendering commands based on the current state
```

## 7. Object States and Pausing: Common Points of Confusion

### 7.1 `_visible`: Visibility

`_visible` controls the corresponding rendering process. It does not mean updates, collisions, timers, or input stop. Hiding an item does not automatically pick it up or destroy it.

### 7.2 `_active`: Active State

`_active` is used to filter the corresponding update and input processes; it does not mean the object is invisible. For an ordinary `GameObject`, scene rendering checks states such as visibility and destruction rather than using the active state directly as the rendering switch.

### 7.3 `_destroyed`: Destruction State

`_destroyed` indicates that an object is being prepared for removal. It differs from temporarily hiding or disabling it. An object that only needs to be invisible temporarily does not necessarily need destruction; a one-time item can enter the destruction process after being picked up.

### 7.4 Paused State

While paused, the scene skips objects that are not permitted to update during a pause and stops that scene's physics simulation. Objects allowed to run while paused may continue. Input processing also has separate pause rules.

A pause menu may still need to handle button input and play UI animations. **Do not assume pausing a scene means the entire application has stopped.**

## 8. World Coordinates, UI Coordinates, and Render Positions

### 8.1 Do Not Mix Coordinate Spaces Directly

World coordinates describe the positions of characters, enemies, and items in the game world. Window coordinates must be distinguished from logical rendering coordinates: they do not necessarily correspond one-to-one after window scaling or logical presentation mapping. The engine's input system converts mouse window coordinates to logical rendering coordinates. `Camera::world_to_screen()` returns logical rendering coordinates in the camera viewport, which cannot be treated directly as actual window pixels. UI positions can also be affected by their parent containers' local coordinates and layout.

For example, if the mouse position is in screen coordinates while a potion's rectangle is in world coordinates, you cannot compare them directly for pickup detection:

```cpp
// If mouse_position is in screen coordinates and potion_rect is in world coordinates,
// they cannot be compared directly:
// potion_rect.contains(mouse_position);
```

First use the appropriate camera or coordinate-conversion mechanism to convert the data into **the same coordinate space**, then compare them.

For example:

```cpp
// Screen position corresponding to the object's logical center.
auto screen_center = camera().world_to_screen(object.center());

// Screen-space area corresponding to the object's rendered rectangle.
auto screen_rect = camera().world_to_screen(object.render_rect());
```

### 8.2 `world_rect()` vs. `render_rect()`

`world_rect()` represents an object's rectangle in world space. `render_rect()` may include a display offset produced by physics interpolation.

When the corresponding interpolation mechanism is operating normally, the engine calculates this display offset automatically. **You do not need to update it manually.**

**Use logical state for gameplay rules and the appropriate presentation state for rendering.** Do not write an interpolated position intended for visual smoothing back into gameplay or physics state.

## 9. Complete Example: An In-Game Pickup, `HealthPotion`

Suppose the game needs the following behavior: the player sees a health potion on the map, meets the pickup conditions, restores health, and the potion disappears from the world. If the game has an inventory, the pickup could instead add the item to it.

### Step 1: Define the Object

```cpp
class HealthPotion : public GameObject
{
public:
    HealthPotion()
        : GameObject(elysia::core::DepthLayer::Item, 0)
    {
    }

    // Implement rendering, pickup-related behavior, etc. as needed.
};
```

It inherits from `GameObject` because it is an item in the game world. This does **not automatically create** a pickup collider, input interaction, or per-frame updates. Integrate these capabilities explicitly according to the chosen implementation. A stationary item that only waits for interaction might not need per-frame updates at all.

### Step 2: Add the Object to a Scene

Using the default constructor defined above:

```cpp
HealthPotion* potion =
    scene.create_and_add_object<HealthPotion>();
```

Use the current, official `GameObject` interfaces to configure its world rectangle, rendering settings, and so on. Once successfully added, the scene owns the object; the returned pointer is only an access point.

### Step 3: Implement Rendering and Interaction as Needed

The potion can submit the appropriate rendering commands. The game layer chooses how to detect pickups, such as using a trigger area, interaction system, or object query.

If the item needs behaviors such as bobbing up and down or disappearing after a timer expires, implement and register the appropriate update capability as needed. There is no reason to enable per-frame updates unconditionally for every item.

### Step 4: Handle the Pickup

```text
Confirm that the player meets the pickup conditions
        ↓
Restore health or update inventory data
        ↓
Prevent the same potion from granting its effect more than once
        ↓
Mark the potion for destruction: potion->destroy()
```

Pickup logic should prevent an object from being processed more than once. After applying its effect, mark the potion for destruction and leave cleanup to the scene.

### Step 5: Release the Object and Clean Up References

Follow the lifetime rules in Section 4. Check whether interaction targets, selected-object references, or deferred callbacks hold references to the potion, and clear associations that would become invalid before the object is freed.

### Step 6: Update the Inventory UI (If Applicable)

The following responsibilities should be kept separate:

```text
HealthPotion     Item instance in the game world
Inventory        Stores game data such as item types and quantities
InventoryPanel   Displays the inventory
InventorySlot    Displays an individual item
```

When the item is picked up, update the player's inventory data first, then mark the world potion for destruction. The UI reads the inventory data and updates its display. Do not use a world object's `HealthPotion*` directly as a long-lived inventory record; doing so can leave dangling pointers after scene cleanup.

## 10. Quick Reference: Common Misconceptions

| Misconception | Correct understanding |
| --- | --- |
| Inheriting from `GameObject` automatically enables updates | Implement and register the appropriate update capability |
| Inheriting from `GameObject` automatically enables collisions | Configure and integrate the physics or interaction system |
| `destroy()` immediately frees the object | It only marks the object; scene objects are freed at a cleanup point that subsequently runs |
| A non-null raw pointer means the object is valid | It can remain non-null and become dangling after the object is freed |
| Hiding an object stops all its behavior | `_visible` primarily controls the corresponding rendering process |
| An inactive object must be invisible | `_active` and `_visible` are separate states |
| Pausing stops the entire application | Some updates and UI input may continue |
| Exactly one fixed update runs per frame | Zero, one, or multiple fixed updates may run |
| Every timer must use fixed updates | Ordinary timers can generally accumulate `delta` |
| Gameplay rules can be modified during rendering | Separate state updates from visual presentation |
| Mouse coordinates can be compared directly to a world rectangle | First bring them into the same coordinate space |
| `render_rect()` is the physical position | It may include an interpolation offset used only for display |
| Leaving a scene always reinitializes it | A scene may be cached, reused, or reset |

## 11. Checklist Before Development

Before adding an object or implementing a feature, confirm:

- Does the object belong to the game world or the UI?
- Which `Scene` or UI container owns it?
- Do its update, input, or physics capabilities need explicit integration?
- Should its logic run during regular updates, fixed updates, or another system stage?
- When is the object destroyed, and who actually cleans it up?
- Are there raw pointers kept across frames or callbacks that may run after the object is gone?
- Do all position comparisons use the same coordinate space?

## 12. Summary

`Scene` organizes and owns objects, while `GameObject` and `UiElement` serve the game world and UI, respectively. Update, input, and physics capabilities must be explicitly integrated as needed; inheritance alone does not imply that they are available.

Scenes and UI containers manage the lifetimes of the objects they own. Developers must still maintain the validity of references kept across frames and callbacks.

Regular and fixed updates serve different timing models. Rendering should present state, not advance gameplay rules. World, screen, and UI coordinates must be distinguished, and logical positions must not be confused with interpolated display positions.
