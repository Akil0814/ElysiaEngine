# Action Mapping

Action maps convert device states/events into game-defined actions. This document distinguishes continuous values from one-shot deltas and explains cleanup on map replacement.

`InputActionMap` parses only `InputSnapshot`, replaying initial source state/ordered raw events, then reconciling final state. Same-frame press/release and repeated clicks retain action order; frame state expresses final continuous values only.

Games define `InputActionId`, keys, and gamepad maps. [game/input](../../../../game/input/gameplay_input_map.cpp) contains demo maps, not engine protocol. UI actions stay independent.

## Value Semantics

`InputActionDescriptor::semantics` is `State` (default) or `Delta`. Buttons support `State` only. Continuous axes use `ButtonInputBinding`, `AxisInputBinding`, `Axis2DInputBinding`, or `Button2DInputBinding`, with dead zones/summation/clamping.

`Delta` uses `PointerDeltaBinding` to select MouseX/MouseY/WheelX/WheelY, component, scale. It uses event deltas after logical conversion, never reaccumulates frame totals, applies no stick dead zone/[-1,1] clamp/frame time, and emits no Started/Canceled.

```cpp
#include "engine/input/action/input_action_map.h"

// Check success before passing map to the local controller.
bool add_look_delta(elysia::input::InputActionMap& map)
{
    using namespace elysia::input;
    const InputActionId look{"mygame.look_delta"};
    // Return registration results; failed actions cannot be assumed queryable.
    return map.register_action(
        {look, InputActionValueType::Axis2D, 0.5f, 0.2f, InputValueSemantics::Delta},
        {{look, PointerDeltaBinding{PointerDeltaAxis::MouseX}},
         {look, PointerDeltaBinding{PointerDeltaAxis::MouseY, InputActionComponent::Y}}});
}
```

One action cannot mix `State`/`Delta`. Mouse aiming deltas and stick angular speed should be separate actions composed by games. Registration/addition/replacement validate and reject invalid types/non-finite scales.

## Maps in Controllers

Each `LocalPlayerController` activates one map. Construct before registration; afterward replace only through `ControllerService::replace_input_map(handle, map)`. `input_map()` is const only.

Successful validation cancels old input, clears state/events/deltas, and establishes release/neutral gates. Failure preserves the map. Returns `ControllerOperation`; callback Pending needs checking after safe commit. No mutable references or automatic walking/vehicle/spectator stacking are provided.

## Command Cache

Continuous values keep latest results; events preserve count/order. With zero steps events/deltas accumulate across frames, clear after the next actual step, and become zero on later catch-up steps. UI consumption, cancellation, unbinding, and map changes clear related pending input.

Maximum 1,024 pending events/controller. Overflow cancels the whole pending input and logs rather than truncating attacks. Non-finite states/events/accumulated deltas cannot enter delivery caches.

Complex chords, map-stack priorities, persistence, and relative mouse mode are not implemented.

## Precision and Parameter Validation

Raw/UI wheel values are float; WheelX/WheelY retain fractions. Scroll containers move proportionally; synthesized gamepad scrolling keeps its existing whole-step cadence. Button/Axis1D bindings accept X only; Axis2D accepts Y. Invalid enums/axis ranges/non-finite scales reject without changing original maps.
