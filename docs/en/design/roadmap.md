# Engine Roadmap

This document records implemented capabilities and future directions to clarify engine boundaries. It promises neither delivery dates nor interface stability. Use guides for usage and architecture documents for implementation.

## Planned Capabilities

- Shader effects.
- Particle system.
- [Networking](player-host-networking-and-input-design.md). ENet is currently a transport dependency; network sessions/state replication are not implemented.

## Directions Requiring Evaluation

Multithreaded physics, editor integration, and texture handle abstraction still require evaluation.

## Implemented Foundations

- Scene lifetime, cached reentry, safe mutation phases, exception recovery.
- Local input routing, keyboard partitions, controllers, fixed-step command delivery.
- Retained UI and world text, value bars, passive bubbles.
- SDL3 migration and Box2D integration.
- Camera following, motion, slot switching/blending.
- Built-in/project resources, runtime configuration, typed saves, failure recovery.
- Project audio, animation, registered effects.
- Module demos, automated tests, optional development overlay.

See the [architecture index](../architecture/README.md) for current subsystems.
