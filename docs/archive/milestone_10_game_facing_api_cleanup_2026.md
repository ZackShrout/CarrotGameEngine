# Carrot Game Engine - Milestone 10

**Last Updated:** April 9, 2026  
**Title:** Game-Facing API Cleanup  
**Status:** Complete  
**Focus:** Reduce game-side boilerplate, move reusable orchestration into engine-owned systems, and separate the application host from the real game runtime so Carrot develops toward a clean gameplay API surface suitable for both C++ game code and future scripting layers.

---

## Milestone Goal

Carrot now has enough real engine functionality that the main problem is no longer just “missing systems.”

The problem is now twofold:

- too much reusable engine behavior still leaks into game code
- too many different runtime roles are still combined inside the `ce_application_t` subclass

Milestone 10 is therefore about two related cleanups:

1. **Capability cleanup**
   Move reusable scene/runtime/input/interaction orchestration into the engine so game code expresses intent rather than sequencing.

2. **Lifetime and ownership cleanup**
   Separate the low-level application host from the actual game runtime and gameplay session objects so the sandbox stops acting like an everything-object.

This milestone is about making Carrot feel more like:

- `audio::play("sfx.door_open")`
- `scene::load("scene.sandbox.town")`
- `scene::transition("scene.sandbox.inn", "InnEntry")`

and less like:

- manually wiring multiple helpers just to load a scene correctly
- game code knowing which engine subsystems must be refreshed in what order
- one giant app object owning application shell, game runtime, scene runtime, controllers, transitions, triggers, and input choreography all at once

---

## Scope Summary

Milestone 10 is **not**:

- a Python scripting integration milestone
- a full prefab/archetype system milestone
- a final ECS milestone
- a full editor tooling milestone
- a complete gameplay framework for all genres
- a rewrite of every gameplay-facing system in one pass

This milestone **is**:

- a cleanup and consolidation milestone
- a game-facing API design milestone
- an engine boundary tightening milestone
- a runtime ownership/lifetime architecture milestone
- a foundation milestone for future scripting-safe APIs

---

## Why This Milestone Comes Next

Carrot has reached the point where the main architectural risk is no longer only “missing engine features.”

It is now also:

- too much engine knowledge leaking into game code
- too much repetitive scene/runtime glue living in the sandbox
- too many gameplay-facing flows depending on helper files or sandbox-local orchestration
- too many different lifetimes and responsibilities combined inside one `ce_application_t` subclass

This matters immediately for C++ usability.

It also matters strategically for future scripting.

If a future Python layer is ever added, Carrot cannot expose low-level engine sequencing, raw orchestration details, or broad internal subsystem knowledge directly to scripts.

That means Carrot needs a cleaner gameplay capability layer first.

This milestone is therefore not just polish.
It is a structural step toward:

- smaller game code
- safer public engine APIs
- stronger authored-data-driven behavior
- future scripting integration
- better long-term maintainability
- a clearer separation between app host, game runtime, and active gameplay state

---

## Closeout Summary (April 9, 2026)

Milestone 10 is now considered complete.

Delivered outcomes:

* engine-owned scene runtime orchestration with `scene::load(...)`, `scene::transition(...)`, scene runtime context, and non-destructive staged scene loading
* engine-owned authored interaction decoding and interaction outcome capture
* engine-owned gameplay input routing, UI-consumption enforcement, and controller interaction dispatch
* engine-owned trigger monitoring and authored trigger event shaping
* engine-owned runtime framework seams with `game_runtime_t` and `igame_state_t`
* sandbox-side lifetime split into:
  * thin `sandbox_t` application adapter
  * `sandbox_game_t` runtime root
  * `gameplay_state_t` active gameplay session
* sandbox project/layout cleanup so the example game now lives honestly under `src/Sandbox/`
* reliability improvements aligned with the new runtime-facing flow:
  * non-destructive failed scene loads
  * defensive asset discovery on bad mount roots
  * Linux auto-backend fail-fast instead of silently choosing unsupported X11

Out of scope by design (deferred):

* Python scripting itself
* a full engine-managed continuity/serialization system replacing game-specific gameplay runtime state
* broader prefab/archetype and editor workflow work
* X11 backend implementation and Wayland client-side decoration work

---

## Problem Statement

The current sandbox code revealed two different architectural issues.

### 1. Mechanism Leaks

Too much reusable engine behavior was sitting in `src/Sandbox`:

- scene loading needed follow-up glue
- authored interaction meaning was decoded game-side
- input routing behavior was partially game-owned
- trigger overlap monitoring and event shaping were game-owned
- interaction dispatch to the controller was game-owned

In short:

> Carrot exposed too much mechanism and not enough capability.

### 2. Lifetime and Role Collapse

Even after some of that behavior moved inward, `sandbox_t` still felt wrong because it combined too many roles:

1. application adapter
2. game bootstrap
3. input wiring
4. gameplay orchestration
5. scene/runtime flow
6. gameplay state ownership

That is not a failure. It is a normal bring-up artifact.

The engine had one obvious integration point, so the sandbox class naturally accumulated responsibilities while systems were being proven out.

But Carrot is now mature enough that this shape is no longer desirable.

In short:

> `sandbox_t` currently acts like the application host, the game runtime root, and the active gameplay session all at once.

That is the other half of the cleanup this milestone must solve.

---

## Architectural Direction

### 1. Capability-First Game API

Game code should primarily express:

- what scene to load
- what sound to play
- what transition to request
- what authored object to query or act on
- what optional override to apply

Game code should not need to manually orchestrate:

- scene bootstrap order
- runtime rebinding steps
- camera policy application
- scene music refresh timing
- helper-driven transition validation plumbing
- whether UI-consumed input reaches gameplay controllers
- how trigger overlap bookkeeping works

---

### 2. Engine-Owned Reusable Runtime Behavior

The engine should own reusable runtime capability such as:

- scene loading and transition machinery
- scene runtime context and controller rebinding
- authored interaction resolution
- gameplay input routing and UI consumption behavior
- trigger overlap tracking and authored trigger event shaping

The engine should make these the default path.

Advanced users may still override pieces intentionally, but not as the default experience.

---

### 3. Capability-First Public API Over Internal Services

Carrot should increasingly expose stable gameplay-facing capability APIs rather than broad internal subsystem access.

For game code, the preferred public shape is:

- `audio::play(...)`
- `scene::load(...)`
- `scene::transition(...)`
- `ui::show(...)`

These public entry points should represent engine capabilities, not service plumbing.

Internally, Carrot may still use service/module-style implementation such as:

- `scene_service`
- `transition_service`
- `world_service`
- `audio_service`
- `ui_service`

Game code should depend on clean capability namespaces while the engine remains free to implement those through internal services/modules.

---

### 4. Application Host vs Game Runtime Root

Carrot needs a clearer split between:

#### Application host

This is the thing the engine talks to.

It answers:

- start
- tick
- input callbacks
- focus/window callbacks

This is the conceptual role of `ce_application_t` and the sandbox subclass that implements it.

#### Game runtime root

This is the thing the application host owns and delegates to.

It answers:

- what runtime state is active
- what scene/world is loaded
- what controller stack exists
- what game flow is happening
- how gameplay updates

This should not be the same object as the low-level application host.

---

### 5. Explicit Lifetime Layers

A strong ownership rule for this milestone:

> put systems where their lifetime naturally matches

#### Application lifetime

Examples:

- application adapter
- low-level engine host boundary

#### Game runtime lifetime

Examples:

- active state stack or current state
- game-wide input config and policy
- persistent runtime composition

#### Gameplay session lifetime

Examples:

- scene runtime
- player controller
- interaction controller
- gameplay runtime state
- trigger monitor
- camera follow behavior

#### Scene lifetime

Examples:

- current scene context
- spawn markers
- scene-local bindings
- loaded world-specific authored anchors

The current sandbox class contains members from at least three of these lifetimes.
That is why it feels architecturally blurry.

---

### 6. Data-Driven Defaults, Optional Overrides

If the engine needs configuration to do the right thing, the preferred order should be:

1. authored scene/world data
2. engine defaults
3. optional explicit overrides for advanced use cases

This principle should apply to:

- player spawn behavior
- camera setup
- scene music
- interaction conventions
- transition targets
- UI input routing behavior

The common case should not require code-heavy setup.

---

### 7. Script-Safe API Direction

This milestone should explicitly treat future scripting as a design constraint.

A good rule:

> if an API would be unsafe or inappropriate to expose to Python, it is probably also too low-level for ordinary game code.

That does not mean “design for Python syntax now.”
It means:

- reduce internal leakage
- return safer handles/contexts
- keep capabilities high-level
- avoid exposing raw engine ownership patterns

The goal is not to mirror C++ internals in Python later.
The goal is to ensure the public gameplay-facing API is already narrow and safe enough that a future scripting layer could sensibly wrap it.

---

## Delivered Work

The following meaningful improvements landed during Milestone 10.

#### Scene runtime cleanup

Completed work:

- introduced an engine-owned scene runtime surface
- added `scene::load(...)`
- added `scene::transition(...)`
- added a scene runtime context
- moved the normal scene-enter path behind engine-owned orchestration
- staged scene loads so failed scene loads do not destroy the current world

Representative files:

- `src/Engine/Scene/Scene.h`
- `src/Engine/Scene/Scene.cpp`
- `src/Engine/World/SceneLoader.cpp`

#### Authored interaction cleanup

Completed work:

- moved authored interaction decoding into the engine
- introduced engine-owned authored interaction helpers/outcomes
- removed sandbox-local interaction helper files
- moved interaction outcome capture into the base engine interaction controller
- removed the sandbox-specific interaction controller adapter

Representative files:

- `src/Engine/World/AuthoredInteractions.h`
- `src/Engine/World/AuthoredInteractions.cpp`
- `src/Engine/World/Controllers/InteractionController.h`
- `src/Engine/World/Controllers/InteractionController.cpp`

#### Input routing cleanup

Completed work:

- introduced an engine-owned gameplay input router
- moved UI-consumption and gameplay suppression decisions into the engine
- moved interaction dispatch into the engine router
- removed game-side input-routing bookkeeping

Representative files:

- `src/Engine/Input/GameplayInputRouter.h`
- `src/Engine/Input/GameplayInputRouter.cpp`

#### Trigger cleanup

Completed work:

- introduced an engine-owned trigger monitor
- moved active trigger bookkeeping into the engine
- moved authored trigger event shaping into the engine
- removed sandbox-local trigger state containers and trigger event types

Representative files:

- `src/Engine/World/TriggerQuery.h`
- `src/Engine/World/TriggerQuery.cpp`

#### Robustness fixes aligned with milestone direction

Completed work:

- scene loading is now non-destructive on failure
- asset discovery now degrades safely on bad mount roots
- Linux auto backend selection now fails fast instead of silently choosing unsupported X11

These were not the main point of Milestone 10, but they improved the reliability of the new runtime-facing flows.

---

## Updated Target Architecture

The current target shape for the sandbox side should be:

### `ce_application_t`

Engine-facing low-level application interface.

Responsibilities:

- startup callback
- tick callback
- input callback surface
- focus/window callback surface

This is a good engine boundary and should remain.

### `sandbox_t`

Thin application-facing adapter.

Responsibilities:

- inherit from `ce_application_t`
- store the engine-provided game context pointer or reference needed for setup
- create the real game runtime root
- forward tick/input/window callbacks into that runtime root

This should not own most gameplay/session systems.

### `sandbox_game_t`

Top-level game runtime root.

Responsibilities:

- startup of the game layer
- own game-wide composition
- configure input mappings and game-wide policy
- select and own the active game state
- forward updates and callbacks into the active state

This is the right place for game-runtime-lifetime responsibilities.

Carrot should provide an engine-owned `game_runtime_t` base class for this shape.
Individual games should then provide their own concrete runtime root such as `sandbox_game_t`.

### `igame_state_t`

Explicit runtime state interface.

This should exist intentionally even if it starts minimal.
It should be engine-owned as part of the gameplay/runtime framework layer rather than redefined inside each game project.

Responsibilities today:

- provide a documented seam between game runtime root and active state
- define the basic lifecycle and callback shape

Responsibilities over time:

- state enter/exit
- state tick/update
- input callback surface
- optional suspend/resume behavior
- support for menu/gameplay/pause/loading/debug states

This should be documented clearly as intentional architecture, not accidental abstraction.

### `gameplay_state_t`

Active in-world gameplay session.

Responsibilities:

- own scene runtime
- own player controller
- own interaction controller
- own gameplay runtime state
- own trigger monitoring and gameplay-scene event reaction
- handle scene load/transition flow for gameplay
- perform gameplay-world updates such as camera follow and runtime-state application

This is where most of the current gameplay-heavy sandbox members naturally belong.

---

## Engine vs Game Responsibility Line

As more code moves out of `sandbox_t`, it is important not to move everything into the engine blindly.

The correct rule is:

- move behavior into the engine when it is **generic capability**
- move behavior into better-structured game/runtime objects when it is **game policy or gameplay meaning**

### Engine should own

- generic scene runtime machinery
- generic scene transition API
- generic input systems and input routing enforcement
- generic controller framework
- generic trigger monitoring and authored trigger event shaping
- generic collision/trigger query support
- generic camera utilities
- generic UI framework

### Game should own

- which scene loads first
- what spawn marker to use
- what a trigger means for this game
- what to do before/after a scene change for this game
- how this game restores runtime state
- how this game chooses state transitions
- how this game composes menus, gameplay, pause, or debug flows

A useful test:

> if this system only exists while gameplay is active, it probably belongs in `gameplay_state_t`, not in the application host and not necessarily in the engine.

---

## Ticket Closeout

Milestone 10 was executed as focused slices rather than one giant rewrite.

### M10-01: Preserve and tighten capability-first public API direction

Target outcome:

- capability namespaces remain the public-facing style
- internal services remain implementation details

Scope:

- keep `audio::play(...)` style public API
- keep `scene::load(...)` and `scene::transition(...)` style public API
- ensure new public entry points prefer capability naming over service-shaped naming

Status:

- complete

---

### M10-02: Finish engine extraction of reusable gameplay orchestration

Target outcome:

- remaining generic scene/input/interaction/trigger machinery is engine-owned
- game code reacts to outcomes rather than sequencing the pipeline

Completed so far:

- scene runtime flow
- interaction resolution
- input router
- interaction dispatch
- trigger monitor

Remaining likely candidates:

- more reusable container/runtime-state hooks if a generic seam emerges
- further generic UI input policy handling

Status:

- complete

---

### M10-03: Split application host from game runtime root

Target outcome:

- `sandbox_t` becomes a thin `ce_application_t` adapter
- `sandbox_game_t` becomes the real game runtime root

Likely work:

- introduce engine-owned `game_runtime_t`
- introduce `sandbox_game_t` on top of that base
- move input config/setup out of `sandbox_t`
- move callback handling logic so `sandbox_t` mostly forwards into `_runtime`
- reduce `Sandbox.h` to app-shell responsibilities only

Primary success criteria:

- `sandbox_t` no longer owns most gameplay/session systems
- `sandbox_t` mostly stores `game_context_t*` plus a runtime object

Status:

- complete

---

### M10-04: Introduce `igame_state_t` explicitly

Target outcome:

- game runtime state shape is explicit and documented
- future gameplay/menu/pause/loading/debug states have a clear seam

Important note:

This interface should exist intentionally even if it begins as a minimal no-op or near-no-op abstraction.
The purpose is architectural clarity, lifetime separation, and future extensibility.
It should be engine-owned so the engine/framework communicates the intended runtime shape to game developers directly.

Initial likely scope:

- virtual destructor
- `enter()`
- `exit()`
- `tick(float)`
- callback methods for input/window events with empty default implementations

Future likely scope:

- optional `suspend()` / `resume()`
- state transition coordination
- stack or layered-state support if needed

Primary success criteria:

- the doc explains why this interface exists
- the codebase contains it explicitly
- it forms the seam between `sandbox_game_t` and active runtime states

Status:

- complete

---

### M10-05: Introduce `gameplay_state_t` as the active in-world session object

Target outcome:

- gameplay-session-lifetime systems are grouped together under one explicit object
- `sandbox_t` and `sandbox_game_t` stop owning gameplay-world details directly

Likely members to move:

- scene runtime
- player controller
- interaction controller
- gameplay runtime state
- trigger monitor
- scene transition/runtime callbacks
- camera follow behavior
- runtime event consumption

Primary success criteria:

- gameplay-world systems live under a gameplay session object
- scene/session logic is no longer hanging directly off the app adapter

Status:

- complete

---

### M10-06: Refactor sandbox to prove the host/runtime/state split

Target outcome:

- the sandbox becomes a clean proof of the intended architecture

Likely work:

- `main.cpp` stays simple
- `sandbox_t` becomes the app adapter only
- `sandbox_game_t` becomes runtime root
- `gameplay_state_t` owns gameplay session systems
- callback forwarding becomes straightforward and readable

Primary success criteria:

- `Sandbox.cpp` stops looking like a mashup of application shell, game runtime, and gameplay state
- the sandbox becomes a clearer example for future games built on Carrot

Status:

- complete

---

### M10-07: Public API consistency and audio hardening pass

Target outcome:

- existing good public API patterns become clearer reference models for the rest of the engine

Likely work:

- explicitly treat `audio::play(...)`, `audio::pause(...)`, `audio::resume(...)`, and `audio::stop(...)` as the preferred public API style
- preserve internal audio service/module plumbing as an implementation detail
- slightly harden the public audio interface so it remains safe and predictable as the engine grows

This is not intended to be a major audio rewrite.

Status:

- complete in principle; the milestone reaffirmed `audio::play(...)` style capability APIs as the reference public shape, and no major audio rewrite was required for milestone success

---

## Post-Milestone Follow-On

Natural follow-on work after Milestone 10:

1. continue tightening `gameplay_state_t` and game-specific runtime-state ownership where future features demand it
2. evolve `GameplayRuntimeState` toward a longer-term engine-managed, gameplay-directed continuity/serialization model when that system is earned
3. continue Linux platform work separately, especially X11 support and Wayland client-side decoration fallback

---

## Definition of Done (Milestone-Level)

Milestone 10 should be considered complete when:

- scene loading and scene-enter runtime setup are exposed as a single clean engine-facing operation
- common authored interaction and trigger behavior are engine-owned where appropriate
- gameplay input routing authority is engine-owned
- the sandbox no longer needs multi-step scene/bootstrap/helper orchestration for the normal path
- `sandbox_t` is a thin application host adapter rather than the owner of all gameplay systems
- `sandbox_game_t` exists as the real game runtime root
- `igame_state_t` exists explicitly and is documented as intentional architecture
- gameplay-session-lifetime systems live under a gameplay state object rather than the app adapter
- the resulting gameplay-facing API feels reasonable to expose to a future scripting layer
- tests cover the new runtime flow and cleaned integration boundaries

---

## Out of Scope By Design

The following are important, but should remain out of scope unless directly required:

- shipping Python scripting itself
- full prefab/archetype authoring pipeline
- full save system architecture
- final editor-driven game workflow
- broad ECS overhaul
- replacing all low-level engine APIs with high-level wrappers

This milestone is about cleaning the **game-facing path** and the **runtime ownership model**, not erasing low-level engine power.

---

## Success Criteria

At the end of this milestone, a game developer should be able to look at Carrot and feel:

- the engine handles the obvious reusable orchestration for me
- authored scene data actually drives the normal path
- the app host is separate from the game runtime
- the game runtime is separate from the active gameplay session
- I can still override behavior when I need to
- I do not need to understand engine internals just to load a scene and play a game
- the current architecture looks like it could eventually support menus, pause, loading, gameplay, and scripting cleanly

If the sandbox still feels like a pile of glue code after this milestone, the milestone is not complete.
