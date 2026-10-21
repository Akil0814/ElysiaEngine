# Player Host Networking and Input Architecture Design

> Input extension: physical keyboard/mouse are now separate; keyboard partitions support local two-player input, and mouse ownership is independent. UI uses keyboard/mouse and an independently designated gamepad rather than game player ownership. Controllers, generic commands, and fixed-step architecture remain. The single UI player owner in historical discussion below is no longer current API. ENet integration and network replication remain unimplemented.

> Status: networking target design/discussion record; local input routing is implemented, network modules are not.
>
> This records goals, purposes, direction, and migration impacts for Player Host networking/unified input. Section 2 preserves the pre-refactor audit. Implemented scope is stated below; remaining networking sections describe targets. Proposed types/interfaces/flows are not usable existing APIs. See [Input documentation](../architecture/subsystems/input/README.md) for current input; other networking interfaces here remain proposals.

This document is for maintainers discussing future networking. It first states implemented local input scope, then retains the historical audit/proposal. Only explicitly implemented sections describe current capabilities.

## Implemented Input Refactoring Status

Local input now uses independent device snapshots, `LocalPlayerRegistry`, `SceneInputRouter`, `ControllerService` / `ControllerManager`, `SceneControlContext`, generic `ControlCommand`, and fixed-step consumption. Repository actors/examples migrated. Old broadcasts were removed; external games must migrate. Single-player needs no network session.

Local game sessions explicitly bound controller lifetime, separately from network sessions. Controllers have scene/Session scopes; cross-scene instances still clear commands/unbind. Ordinary scenes create no control context; `GameplayScene` creates no controllers automatically. Games define actions/maps. Mouse movement/wheel are deltas consumed once per fixed step.

Confirmed local scope includes single shared UI control, explicit device binding, and release/neutral recovery after suppression. The two-player example shares a camera. Network sessions/identity/replication/prediction, split-screen, and personal focus trees are absent.

Historical audit below preserves pre-refactor context, not current input API. See Input documentation for migration/acceptance.

## 1. Background, Goals, and Scope

Future ENet-based Player Host networking has one player host both authority and local play, with others connecting. Host authority means the Host determines final world state while clients submit operations and receive results.

A unique `std::uint64_t` `GameObject` ID identifies objects but does not determine control permissions, damage authority, client creation, or input timing. Independently incremented IDs do not identify the same entity across machines. Preparation must cover identity, lifetime, ownership, simulation timing, and replication.

Goals and purposes:

| Goal | Purpose |
| --- | --- |
| Shared command execution for single-player/networking | Avoid separate rules for local, Host, and remote players |
| Route player input to actors instead of broadcasting | Explicit ownership, actor switching, future local multiplayer |
| Separate/coordinate UI and gameplay | Immediate UI without attacks while typing or using menus |
| Consume commands at fixed simulation steps | Prevent render rate from changing one-shot counts |
| Host determines authoritative state | Avoid divergent damage/spawn/destruction |
| Optional network identity/session | Single-player needs no connections; decorations/UI need no replication responsibilities |
| Explicit migration boundaries | Distinguish network independence from zero source changes |

Scope targets basic Player Host networking without requiring a separate dedicated server first. Host migration, prediction, and rollback are not prerequisites. Mid-game join, reconnect, and public connectivity remain undecided, not committed features.

## 2. Pre-Refactor Implementation and Reasons (Historical Audit)

This reflects source inspected before input refactoring. Links now target current files, whose broadcast/fixed-step portions have been replaced.

| Pre-refactor implementation (historical) | Impact on target design |
| --- | --- |
| [Root CMake](../../../CMakeLists.txt) includes ENet; [engine CMake](../../../engine/CMakeLists.txt) links `enet` | Build dependency exists, not connection management, command protocol, or replication runtime |
| [GameplayScene](../../../engine/gameplay/scene/gameplay_scene.cpp) converts maps to `GameplayInputFrame`, broadcasts to eligible receivers, dispatches consumable events in receiver order | No explicit player-to-actor mapping; broadcasts cannot distinguish local/remote actors |
| [Scene](../../../engine/scene/scene.cpp) performs variable updates before physics | No unified scene-wide fixed-step gameplay/physics contract yet |
| [PhysicsWorld::advance()](../../../engine/physics/physics_world.cpp) invokes registered `PhysicsStepParticipant` | Skills, cooldowns, spawning without bodies also need fixed simulation |
| Object registration/hooks and deferred destruction exist | Network registry needs symmetric removal notifications including whole-scene cleanup |
| [GameObject](../../../engine/core/game_object.h) distinguishes `world_rect()` / `render_rect()` | Presentation interpolation can remain separate from logical state |
| Physics writes body positions back to `GameObject` after steps | `GameObject::set_position()` alone may be overwritten next step |

UI/gameplay already had input separation. The aim is context, player routing, and fixed-step boundaries rather than duplicate device collection.

ENet is transport. Engine/game define identity correspondence, replicated fields, permissions, spawn protocol, and rules; linking it supplies none automatically.

## 3. Unified Input Architecture

### 3.1 From Object Broadcasts to Player Control

The old main path:

```text
Keyboard / mouse / gamepad
    → InputSystem
    → InputActionMap
    → GameplayInputFrame / action events
    → Game input receivers in the scene
```

Target flow:

```text
Keyboard / mouse / gamepad
    → InputSystem
    → Input routing and contexts
        ├→ UI input → focus, buttons, menus, text boxes
        └→ Game action mapping → player controller → player commands → actor
```

Both upper paths share device collection. UI/gameplay should not unconditionally respond together; routing/context determines access.

| Layer | Target responsibility | Excluded responsibility |
| --- | --- | --- |
| `InputSystem` | Collect device states/events | Decide actor attacks/movement |
| Routing/contexts | Coordinate focus, modals, devices, suppression | Calculate damage |
| UI input | Frame-level focus, clicks, navigation, text, scrolling | Network actor commands for ordinary UI |
| Game action maps | Interpret devices as actions | Select remote-controllable objects directly |
| Player controllers | Bind targets, convert/submit commands | Duplicate authoritative rules |
| Command queues/scheduling | Cache/deliver at simulation boundaries | Network callbacks mutating traversed worlds |
| Actor/gameplay logic | Execute movement/skills from commands | Direct keyboard/ENet dependency |

Separate players from controlled actors: switching actors/vehicles should not make actors know whether commands came from devices, network, or tests. This also supports future replay/local multiplayer foundations, without automatically implementing either full system.

### 3.2 Command and Interface Sketch

This proposed example is neither existing types, final public signatures, nor network layout. Games define actions; the engine supplies routing, queuing, and simulation scheduling.

```cpp
// Game-layer sketch, not a directly serializable memory layout.
struct ExamplePlayerCommand {
    std::uint64_t tick;
    std::uint32_t sequence;
    Vector2 move;
    std::uint32_t held_buttons;
    std::uint32_t pressed_buttons;
};

// Proposed call relationships; interface names and handle types are not final.
controller.possess(character_handle);
character.apply_command(command);
```

`tick` relates commands to simulation; `sequence` supports ordering/deduplication. Client-to-Host `tick` mapping, late commands, and wraparound need protocol decisions. Client values are not trusted authoritative clocks.

Movement/jump fields are illustrative. No immutable base protocol can hardcode every game's attacks/skills/interactions. Bitmaps suit mergeable state/actions; preserve repeated quick operations through queues/counts.

### 3.3 Frame Sampling and Fixed-Step Consumption

UI normally responds per frame; game input samples into caches for fixed simulation. Semantics must survive zero/one/multiple steps per frame.

| Situation | Target behavior |
| --- | --- |
| Held movement/defense | Continuous state until release/context cleanup |
| One jump/attack press | Consume once |
| No simulation step | Retain pending actions rather than clear at frame end |
| Multiple catch-up steps | Do not duplicate one press |
| Multiple valid operations before consumption | Queue/count actions needing multiplicity/order |
| Repeated network action | Deduplicate through command identity/consumption records, not just button state |

Continuous state/events remain semantically distinct even if packed into one future message.

### 3.4 UI and Gameplay Coordination

| Context | UI | Gameplay |
| --- | --- | --- |
| Normal play | HUD pointer handling under rules | Controllers receive allowed actions |
| Pause menu/modal | Navigation, confirm, cancel | Suppress related actors; network world pause decided separately |
| Chat focus | Text/editing | Keyboard no longer moves/attacks; other devices follow context rules |
| Focus loss/target switch | Update focus/interaction | Clear/end old held and pending input |

Suppression must terminate existing movement as well as stop new commands. Recovery must not replay menu-period attacks or deliver old actor commands to new targets.

Ordinary hover/menu focus needs no synchronization. World-affecting UI such as start/all-player pause can become explicit session requests/game commands handled by Host.

Per-device suppression, consumption priority, and recovery keys still need interface decisions. This table specifies target behavior, not implementation claims for the historical router.

## 4. Single-Player and Player Host Flow

### 4.1 Three Sources, One Execution Path

```text
Single-player:
Local controller → queue → local world execution → local presentation

Host local player:
Local controller ─────────────────┐
                                 ↓
                     Authoritative queue → Host simulation
                                 ↑             ├→ Host presentation
Remote controller → ENet → Host validation ┘    └→ snapshots → client application/presentation
```

Host local commands queue directly without network loopback but share remote gameplay entry. Do not move/attack in both input callbacks and authoritative simulation.

Host needs one authoritative world and renders it directly. Duplicating scenes to imitate server/client risks double physics/damage/spawns.

Client replicas need explicit duties: display authority only or future prediction. Without prediction design, do not unconditionally run authoritative AI/damage/drops/spawns. Prediction needs validation/correction.

### 4.2 Player Identity and Permissions

Host identifies players from connections, then routes via bindings. Never trust claimed client player/target IDs.

Actor ownership authorizes operation requests, not direct decisions about health/hits/drops/others. Decode/parameter validation belongs to networking; legal gameplay-state validation to authority.

### 4.3 Pause, Menus, and Leaving

Single-player can pause its world. Networking separates local menu, local input suppression, and all-world pause.

Suggested default: Host menus leave authority/network polling running, blocking only that player's relevant input. All-player pause requires explicit Host synchronization while networking continues.

Ending sessions when Host exits is a suggested basic default avoiding migration/authority handoff, not a confirmed product requirement. Client exit clears bindings/network state; games choose actor destruction/retention.

## 5. Network Identity, Lifetime, and Synchronization

### 5.1 Identity Scopes

Except existing `PhysicsObjectHandle`, these are proposed concepts, not current APIs.

| Identity | Purpose | Allocation/boundary |
| --- | --- | --- |
| ObjectId | Process-local identity | Locally generated, need not match machines |
| NetworkId | Same network entity across peers | Host-assigned, reused by clients |
| PlayerId / OwnerId | Player/control ownership | Separate from objects; Host-managed binding |
| `PhysicsObjectHandle` | Local physics reference | Existing physics duty, not network identity |

Network IDs may use `std::uint64_t`, but scope/reuse rules matter more than bit width. Suggested: no reuse within sessions, with session/world generations separating restarted rooms/worlds. Old messages must not alter new objects.

Identity should be optional via components/interfaces/external registries; form is undecided. UI/decorations/local effects need not replicate.

### 5.2 Registration, Spawn, and Destruction

Target lifecycle:

1. Host decides spawn, assigns identity, type, and initial state.
2. Clients invoke factories using stable proposed `TypeId/PrefabId`, never pointers/construction-order assumptions.
3. Register identity, reject duplicates, then apply later state.
4. Host decides destruction/sends lifecycle messages; remove lookup, control, and external references before release.
5. Whole-scene exit/reset and disconnection must deregister too, not only ordinary per-object destruction.

Registries may map `NetworkId → GameObject`, with lifetime-bound references. Marked objects stop accepting valid commands even before memory release.

Receive/decode/validate/queue first; create/mutate/destroy at safe update boundaries. Current addition changes object/interface containers and can invalidate traversal. Future receive threads must also never directly mutate scenes.

Full initial state is fundamental. Mid-game join additionally needs joining full snapshots with later deltas; first-release inclusion remains undecided.

### 5.3 Unified Simulation Timing

Conceptual target order:

```text
Poll/cache networking / collect local input
    → Apply lifecycle changes at safe boundaries
    → Fixed tick: commands → game logic → physics → finish tick state
    → Generate snapshots at send cadence
    → Render interpolation and UI
```

Fixed simulation includes authority without bodies. Gameplay/physics share one time base instead of independent accumulators/catch-up, and old/new callbacks must not duplicate logic.

Simulation rate, rendering rate, and snapshot rate differ; snapshots may be slower. Rates, catch-up limits, overload, and lateness need future decisions. Existing physics limits are not automatically a complete network-clock policy.

### 5.4 Fields and Physics Correction

Synchronize explicit fields such as position, velocity, health, action state, not raw `GameObject`/struct memory. Protocols need encoding, length, version, and validation.

Apply authority through [PhysicsWorld](../../../engine/physics/physics_world.h) `set_transform()` and similar APIs to keep bodies/objects consistent. Velocity preservation/replacement must match received state. `GameObject` position alone is insufficient.

Client interpolation smooths display, not authoritative collision. Physics/network interpolation need explicit composition/selection rather than overwriting one offset and double-smoothing. This depends on remote physics participation and awaits implementation decisions.

### 5.5 Session Module and Transport

Proposed independent `NetworkSession` encapsulates ENet initialization, connections, send/receive, disconnection, decoding, and statistics, confining ENet types to the boundary. Single-player needs no session/loopback for local commands.

Suggested message distinctions:

| Purpose | Suggested strategy/constraints |
| --- | --- |
| Spawn/destruction/critical session control | Reliable, handling duplicate/stale lifecycle messages |
| Continuous snapshots | Possibly unreliable sequenced; newer valid replaces older |
| Player commands | Retransmission/redundancy/deduplication per state/event semantics; packet loss must not permanently lose critical actions |

Separate channels for critical messages/snapshots avoid unnecessary waiting. Channels provide no global business order: snapshots before spawn need bounded caching, ignoring until full state, or another explicit strategy, undecided until protocol design. Late state must not resurrect destroyed entities.

Decode validates type, version, length, ranges, session, and permissions. ENet transport does not solve public reachability, discovery, NAT traversal, or relay; these are separate product/deployment choices.

## 6. Compatibility, Migration, and Implementation Order

### 6.1 Evolution of the Discussion

Initial discussion favored low-intrusion preparation: retain single-player input and add routing/network behavior through optional scenes/runtime to reduce migration.

Later focus shifted to unified input for both single-player/networking through controllers/commands. This favors consistent rules but creates one-time migration cost; zero changes for all old games cannot also be promised.

Separate two goals:

- **Single-player independence:** migrated games need no network session and UI remains local.
- **Old source compatibility:** depends on retaining receivers/adapters; transition duration is undecided, with no unconditional guarantee.

Adding networking still requires game-defined authority/fields. Shared commands are foundations, not automatic networking for every game.

### 6.2 Game Migration Impact

| Old pattern | Target pattern |
| --- | --- |
| Actors receive broadcasts through game input receivers | Controllers bind actors that consume commands |
| Input callbacks move/apply force/attack | Generate/cache intent, execute in fixed simulation |
| Actors read global keyboard | Read supplied commands |
| Every actor receives identical input | Players control bound objects only |
| State resolved throughout code | Separate authority from client presentation |

Temporary old receivers are optional, not permanent architecture. Actors/adapters must not consume old/new paths twice. Formal refactoring must define transition, example migration, and removal.

### 6.3 Suggested Implementation Roadmap

| Stage | Deliverable | Purpose |
| --- | --- | --- |
| 1 | Controllers, command entry, UI/game contexts | Establish ownership/suppression |
| 2 | Fixed consumption and migrated examples | Verify no lost/duplicate actions offline |
| 3 | Identity, lookup, registration/removal | Safe references/network lifetimes |
| 4 | ENet sessions, Host validation, remote commands | Shared authoritative path |
| 5 | Spawn/destruction, snapshots, interpolation | Observable basic networking loop |
| 6 | Join/reconnect/prediction as needed | Extend stable foundations |

This is suggested direction, not authorization to implement those code changes as part of writing this document. Resolve relevant interfaces/policies before each stage.

## 7. Future Acceptance Scenarios

These are future targets, not tests executed/passed in this round.

| Scenario | Expected result |
| --- | --- |
| Offline without network sessions | Input/gameplay/UI/pause work |
| Two players submit movement | Each affects only its actor |
| Attack press without simulation this frame | Retain until a consumable step |
| Multiple catch-up steps | One press executes once |
| Multiple nonmergeable actions | Preserve defined count/order |
| Chat/menu/target switch | No accidental gameplay, held residue, or old actions on new target |
| Host local attack | One authoritative resolution, no local/server duplicate |
| Forged other-player/object requests | Host rejects unauthorized commands |
| Duplicate/out-of-order commands/lost snapshots | No duplicated actions or stale-state regression |
| Interleaved spawn/state/destruction | No unknown-object access/resurrection |
| Release/switch/disconnect | No dangling mappings; old sessions/worlds cannot affect new objects |
| Physics authority correction | Next step does not restore uncorrected positions |
| Physics/network interpolation together | Clear logic/display responsibility, no double smoothing |
| Host local menu | Polling/connections continue |

## 8. Open Decisions and Suggested Defaults

These are not finalized product rules to implement directly.

| Open item | Current suggestion/constraint |
| --- | --- |
| Old input transition period | Temporary adapters possible without double consumption; duration/removal undecided |
| Final API/extensions | Names here illustrative; define game extension/engine boundaries |
| Identity representation | Component/interface/registry undecided; optional required |
| Devices/context priority | Explicit routing; local device management/exact consumption priorities undecided |
| Simulation/snapshot frequency | Shared simulation base, independent send cadence; numbers undecided |
| Input clock/transport | Define `tick` mapping, lateness, event retransmission/deduplication |
| Interpolation/remote physics | Choose participation/display and prevent overwrite |
| Mid-game joining | First-release inclusion undecided; explicitly prohibit if absent |
| Reconnect | Identity/actor retention/full resynchronization undecided |
| Prediction/rollback | Future additions, not prerequisites |
| Host exit | Suggest session end initially; migration not prerequisite, final experience undecided |
| All-player pause | Suggest local menus keep world running; Host synchronizes explicit global pause |
| Public connectivity/NAT | Discovery/direct/traversal/relay separately decided; ENet dependency proves none |

Freeze relevant interfaces/behavior before implementation, then document completed parts in subsystem references. This remains target design, not a substitute for source references using unimplemented illustrative APIs.
