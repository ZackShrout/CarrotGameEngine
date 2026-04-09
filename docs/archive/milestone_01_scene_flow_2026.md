# Carrot Game Engine - Milestone 01

**Title:** Scene Flow Foundation
**Status:** Completed and archived
**Focus:** Move from sandbox-local bootstrap logic to reusable scene loading, transition flow, and test coverage.

Archived in April 2026 after the milestone work landed in the engine and sandbox.

---

## Milestone Goal

This milestone turns Carrot's current working sandbox slice into the first reusable gameplay flow.

Today, the engine already supports:

* asset discovery
* sprite and tilemap loading
* tilemap-backed world import
* player movement
* proximity interaction
* tilemap-authored hybrid objects

What is still missing is the layer that makes those systems feel like a real game foundation rather than a hardcoded demo bootstrap.

This milestone focuses on that missing layer.

---

## Ticket 1 - Scene Definition and Scene Loader

**Priority:** P0
**Outcome:** A world can be created from a scene definition instead of being manually assembled in sandbox bootstrap code.

### Why

Right now, the current overworld flow is real, but it is still glued together in game-side setup code in:

* [SandboxSceneBootstrap.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Sandbox/SandboxSceneBootstrap.cpp)

That is a good temporary step, but it is becoming the main structural limiter.

The engine needs a reusable scene-loading layer that can:

* choose which tilemap to load
* define which marker to use for player spawn
* define which actors should exist at startup
* establish initial camera/presentation policy
* give transitions a stable destination model

### Scope

Add a first scene asset format and scene loader path.

Suggested first format:

* `*.scene.json`

Suggested initial fields:

* scene id
* primary tilemap asset id
* default spawn marker name
* optional player object template or player sprite id
* presentation settings such as pixels-per-unit
* optional initial music asset id

### Likely Touch Points

* [Engine.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Engine.cpp)
* [AssetDiscovery.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Assets/AssetDiscovery.cpp)
* [AssetManager.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Assets/AssetManager.h)
* New scene asset types under `/Users/zshrout/dev/CarrotGameEngine/src/Engine/Assets/Scene/`
* [SandboxSceneBootstrap.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Sandbox/SandboxSceneBootstrap.cpp)

### Implementation Notes

Keep this layer orchestration-focused.

It should not:

* become a full ECS scene graph
* move gameplay meaning into engine internals
* replace tilemap object import logic

It should:

* assemble world state from authored scene data
* call existing asset and world import systems
* preserve game-side override points

### Acceptance Criteria

* The sandbox overworld can load from a `*.scene.json` asset.
* The scene loader instantiates the main tilemap world object.
* The scene loader imports tilemap-authored world objects through the existing world bridge.
* The player spawns from an authored marker rather than hardcoded bootstrap placement.
* Existing sandbox behavior remains functionally intact.

---

## Ticket 2 - Scene Transition Flow and Door Routing

**Priority:** P1
**Outcome:** Door interaction can trigger a real scene or spawn transition instead of only logging intent.

### Why

The current interaction path already proves the authored-object model works:

* signs
* doors
* chests

But door behavior currently stops at logging in:

* [SandboxInteractionController.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Sandbox/SandboxInteractionController.cpp)

That means the engine has object interaction semantics, but not yet actual scene flow.

### Scope

Add a transition path that can:

* unload or reset current world state
* load a target scene
* place the player at a target marker
* support same-scene marker jumps as a simpler first case if useful

Suggested first authored door fields:

* target scene id or target tilemap/scene asset id
* target marker name

### Likely Touch Points

* [SandboxInteractionController.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Sandbox/SandboxInteractionController.cpp)
* [WorldInteractionHelpers.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Sandbox/WorldInteractionHelpers.cpp)
* New scene runtime service or loader entry point
* [World.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/World/World.h) and related reset/clear helpers if needed

### Implementation Notes

Prefer explicit transition requests over hidden side effects.

A good shape would be:

* interaction code requests a transition
* engine/game runtime applies that transition in a controlled point of the frame

That avoids world mutation happening in arbitrary call sites.

### Acceptance Criteria

* Interacting with a door can trigger a real transition.
* Player position after transition is resolved from an authored marker.
* Same-scene and cross-scene routing are both architecturally supported, even if only one is enabled first.
* Transition failures log clearly and fail safely.

---

## Ticket 3 - Automated Tests for Asset and World Import

**Priority:** P1
**Outcome:** The new scene-loading path is protected by repeatable tests instead of only manual sandbox verification.

### Why

Carrot now has enough asset and world infrastructure that manual-only verification is becoming risky.

The highest-value regression surface is the authored-data path:

* asset discovery
* manifest import
* tilemap import
* sprite import
* tilemap-to-world object bridging
* future scene loading

### Scope

Add a first automated test target covering data-driven runtime setup behavior.

Suggested first tests:

* asset discovery finds `audio`, `texture`, `sprite`, `tilemap`, and new `scene` manifests
* tilemap manifest import preserves ids and source references
* tilemap world bridge creates marker and tile objects correctly
* typed properties survive import
* scene loader resolves tilemap and spawn marker correctly

### Likely Touch Points

* New `/Users/zshrout/dev/CarrotGameEngine/tests/` target in [CMakeLists.txt](/Users/zshrout/dev/CarrotGameEngine/CMakeLists.txt)
* Asset system import code
* World bridge code
* Scene loader code from Ticket 1

### Implementation Notes

Favor small fixture assets and deterministic assertions.

These tests should exercise the same public or semi-public paths the engine uses for real loading instead of bypassing the architecture.

### Acceptance Criteria

* A test target exists and builds with CMake.
* Core asset/world import scenarios run without launching the sandbox executable.
* The new scene-loading path has at least one positive-path test and one failure-path test.

---

## Ticket 4 - Input Action Mapping Layer

**Priority:** P2
**Outcome:** Gameplay code depends on semantic actions instead of raw key codes.

### Why

The current sandbox input path is still hardwired to specific keys in:

* [Sandbox.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Sandbox/Sandbox.cpp)

That is acceptable for a sandbox, but it becomes friction once scene flow and broader gameplay logic expand.

The architecture notes already point at this pressure area as one to watch next.

### Scope

Add a lightweight input action layer for gameplay-facing controls.

Suggested first actions:

* `move_up`
* `move_down`
* `move_left`
* `move_right`
* `interact`
* `quit`
* `toggle_fullscreen`

### Likely Touch Points

* [Sandbox.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Sandbox/Sandbox.cpp)
* Input system code under `/Users/zshrout/dev/CarrotGameEngine/src/Engine/Input/`
* Config or authored mapping location if introduced

### Implementation Notes

Keep this simple in the first pass.

The goal is not a giant rebinding UI system yet.
The goal is to create the engine boundary that separates:

* physical input
* gameplay intent

### Acceptance Criteria

* Sandbox gameplay no longer directly depends on raw `W`, `A`, `S`, `D`, and `E` checks for its core logic.
* Movement and interaction still behave the same from the player's perspective.
* Action queries are reusable by future games.

---

## Ticket 5 - Warning and Stub Cleanup Pass

**Priority:** P3
**Outcome:** The codebase is quieter and easier to trust while the new scene layer settles in.

### Why

The current build is healthy and succeeds, but it carries a noticeable amount of placeholder and unused-code noise, especially in:

* audio
* Vulkan backend
* Metal backend

That is normal during engine growth, but trimming the highest-noise warnings will make future work easier to verify.

### Scope

Clean up the most actionable warnings and clearly label intentional stubs.

Good first targets:

* unused variables
* ignored `[[nodiscard]]` results
* obvious placeholder fields that are no longer used
* comments that should explicitly identify temporary backend gaps

### Likely Touch Points

* [AudioEngine.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Audio/Core/AudioEngine.cpp)
* [Delay.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Audio/DSP/Delay.h)
* Vulkan backend files under `/Users/zshrout/dev/CarrotGameEngine/src/Engine/RHI/Backends/Vulkan/`
* Metal backend files under `/Users/zshrout/dev/CarrotGameEngine/src/Engine/RHI/Backends/Metal/`

### Acceptance Criteria

* Warning count is reduced meaningfully in the default local build.
* Remaining placeholder implementations are easier to identify on purpose.
* No functional regressions are introduced during cleanup.

---

## Recommended Execution Order

1. Ticket 1 - Scene Definition and Scene Loader
2. Ticket 3 - Automated Tests for Asset and World Import
3. Ticket 2 - Scene Transition Flow and Door Routing
4. Ticket 4 - Input Action Mapping Layer
5. Ticket 5 - Warning and Stub Cleanup Pass

---

## Rationale for Order

Ticket 1 creates the missing structural layer.

Ticket 3 then protects that layer before transition behavior adds more complexity.

Ticket 2 turns the scene layer into actual gameplay flow.

Ticket 4 improves long-term gameplay architecture once the flow shape is clearer.

Ticket 5 is intentionally last so cleanup follows the new stable structure instead of fighting ongoing design changes.

---

## Suggested Definition of Done for the Milestone

This milestone should be considered complete when:

* the sandbox overworld loads through a scene definition
* at least one authored transition path works
* scene-loading behavior is covered by automated tests
* gameplay input is no longer tightly coupled to raw keyboard branches
* the current build remains green and easier to reason about
