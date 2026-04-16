# Carrot Game Engine - Milestone 21

**Last Updated:** April 15, 2026
**Title:** Gameplay-Facing Runtime API Cleanup and Engine Boundary Hardening
**Status:** Proposed
**Focus:** Strengthen Carrot's game-facing APIs by making runtime, scene, view, controller, and input surfaces cleaner and more intentional while preventing sandbox-specific conventions from quietly becoming permanent engine architecture.

---

## Milestone Goal

Carrot now has a large amount of real engine-facing runtime behavior:

* `game_context_t`
* `game_runtime_t`
* scene runtime and scene transitions
* gameplay-facing controller helpers
* input routing and rebinding
* camera/view surfaces
* world/object interaction flow

That means the next API question is no longer:

* can game code drive the engine at all?

It can.

The next API question is:

* whether game-facing code is being guided toward permanent engine seams
* whether recurring patterns still live in sandbox code because the right engine API surface has not been formalized yet
* whether engine code and game code have a clear boundary of responsibility
* whether "the way Sandbox currently does it" is accidentally becoming the public contract

Milestone 21 exists to make Carrot easier to build games on without turning it into an over-prescriptive gameplay framework.

This milestone is successful if Carrot ends with:

* cleaner gameplay-facing runtime APIs
* clearer expectations for game-side ownership versus engine-side ownership
* reduced sandbox leakage into engine architecture
* stronger, more reusable game/runtime seams
* fewer places where future games would need to imitate Sandbox internals just to use the engine correctly

---

## Scope Summary

Milestone 21 is:

* a gameplay-facing API cleanup milestone
* an engine/game boundary milestone
* a runtime usability milestone
* a reuse and ownership milestone

Milestone 21 is not:

* a giant gameplay framework milestone
* a quest/dialogue system milestone
* a save/load milestone
* a full scripting milestone
* a "put all game logic into the engine" milestone

The key rule is:

**The engine should provide strong reusable seams without stealing game-specific meaning.**

---

## Why This Milestone Comes Next

Carrot has enough real runtime architecture that API shape now matters as much as feature presence.

Current strengths:

* game code no longer needs to talk to raw engine internals for everything
* scene/runtime/view/controller/input seams are already real
* reusable controller helpers exist
* runtime state and diagnostics are stronger than they were a few milestones ago

Current risks:

* Sandbox remains the strongest proof of correct usage, which is useful but dangerous if left unexamined
* some engine-facing runtime expectations still have to be inferred by reading Sandbox flow
* recurring patterns may still be too local to be safely reused by another game without copying structure
* game-facing APIs can still drift toward "whatever current engine internals expose" instead of deliberate contracts

This is the right point to harden the public side of the runtime before more gameplay-facing systems arrive.

---

## Core Architectural Rule

Engine APIs should carry reusable structure, not game-specific policy.

That means:

* engine surfaces should expose durable concepts such as scene transition, controller binding, view control, and runtime state
* game code should remain responsible for game meaning, authored conventions, and outcome semantics
* the engine should not require future games to understand Sandbox-specific glue just to use its systems correctly

If a future game would need to copy Sandbox patterns because the engine API is too thin or too incidental, this milestone has not gone far enough.

---

## Primary Deliverables

### 1. Gameplay-Facing Runtime API Audit

Audit the current engine/game boundary around:

* `game_context_t`
* `game_runtime_t`
* `igame_state_t`
* scene runtime access
* view/camera control
* controller helpers
* interaction flow
* input routing surfaces

Required outcomes:

* identify which current APIs are already durable
* identify which still feel like internal seams leaking outward
* tighten the most important unstable boundaries

### 2. Stronger Runtime and View Surfaces

Carrot should make game-facing runtime and view interactions feel more intentional.

Required outcomes:

* clearer guidance on what game code should do through runtime/view APIs
* fewer places where gameplay code needs to know too much about engine internals
* stronger separation between renderer internals and view-level game controls

### 3. Controller and Interaction API Cleanup

The controller layer is one of Carrot's most promising reusable runtime surfaces.
It should be made cleaner before it grows larger.

Required outcomes:

* stronger and clearer contracts around player controller and interaction controller responsibilities
* clearer engine hooks versus game override points
* fewer hidden assumptions tied specifically to current sandbox content

### 4. Input Routing and Runtime Ownership Cleanup

The input stack already does real work and should now become easier to reuse by another game.

Required outcomes:

* cleaner game-facing routing configuration expectations
* clearer ownership of UI/debug/gameplay input concerns
* stronger documentation and diagnostics for intended use

### 5. Sandbox Leakage Audit

This milestone should explicitly review where Sandbox is still teaching the engine what its API "really" is.

Required outcomes:

* identify patterns that should become engine-owned helpers
* identify patterns that should remain game-specific and be documented as such
* remove or reduce accidental API shape defined only by Sandbox usage

---

## Ticket Breakdown

### Ticket 21.1 - Runtime API Responsibility Audit

Review and tighten the public runtime API surface.

Deliverables:

* stronger guidance and cleanup around game/runtime/state/view access
* removal of the most misleading or incidental public patterns where practical

### Ticket 21.2 - Game View and Camera API Cleanup

Make view/camera interaction more clearly game-facing and less renderer-internal in flavor.

Deliverables:

* cleaner view-level control surface
* clearer relationship between scene runtime camera defaults and game-side view usage

### Ticket 21.3 - Controller Helper Contract Cleanup

Harden the controller layer as a reusable engine concept.

Deliverables:

* clearer contracts for move/facing/collision/interaction semantics
* clearer override points and extension points

### Ticket 21.4 - Input Routing Usability Cleanup

Improve the clarity of engine-facing input routing usage.

Deliverables:

* cleaner intended usage for routing modes and player contexts
* better diagnostics or doc support where the current flow is easy to misunderstand

### Ticket 21.5 - Sandbox Leakage Reduction

Explicitly audit where engine API truth still depends too heavily on Sandbox proof.

Deliverables:

* move truly reusable patterns into engine code where justified
* leave game-specific meaning in Sandbox where it belongs
* update docs to preserve the line

### Ticket 21.6 - Regression Coverage and API Examples

Add or improve tests and examples around the cleaned surfaces.

Deliverables:

* tests for the most important gameplay-facing contracts
* stronger docs or sample usage where API intent is subtle

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. a clearer gameplay-facing runtime API boundary
2. cleaner view/camera and controller usage surfaces
3. reduced reliance on implicit Sandbox knowledge to use core runtime systems correctly
4. better documentation and tests around intended usage

If these land cleanly, the milestone succeeds even if:

* broader optional gameplay modules remain future work
* deeper multiplayer/game-framework features are still deferred

---

## Closeout Criteria

Milestone 21 is complete when:

* Carrot's game-facing APIs are cleaner and easier to reason about
* engine ownership versus game ownership is clearer
* Sandbox no longer defines as much of the practical public contract by implication
* reusable patterns have been promoted where justified and kept local where not

Completion does not mean the engine now owns gameplay.
It does mean game code has stronger permanent seams to build on without copying engine-adjacent glue.
