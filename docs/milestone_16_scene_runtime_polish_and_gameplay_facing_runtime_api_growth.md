# Carrot Game Engine - Milestone 16

**Last Updated:** April 13, 2026  
**Title:** Scene Runtime Polish and Gameplay-Facing Runtime API Growth  
**Status:** Draft  
**Focus:** Turn Carrot's current scene/runtime flow into a more intentional, presentation-ready, gameplay-facing engine layer with explicit async scene transitions, cleaner camera/runtime ownership, and stronger reusable runtime APIs for scene-driven games.

---

## Milestone Goal

Milestones 10 through 15 gave Carrot real engine foundations:

* scene loading and scene transitions
* world loading and spawn routing
* camera follow behavior
* authored interaction outcomes
* trigger monitoring
* collision and movement foundations
* runtime UI and text rendering
* user-facing input rebinding and first-pass broader routing
* forward+ world rendering

That means the next bottleneck is no longer "can Carrot do scene-based gameplay at all?"

It can.

The next bottleneck is:

* how polished and intentional scene runtime flow feels
* how much reusable gameplay runtime structure still lives only in sandbox code
* how cleanly scene presentation, camera ownership, and transition lifecycle are exposed to game code
* whether the engine gives games strong runtime APIs instead of only low-level scene/world primitives

Milestone 16 exists to address that layer directly.

This milestone is successful if Carrot ends with:

* cleaner engine-owned scene runtime lifecycle and context
* explicit async scene transition architecture
* transition presentation hooks that can support fades, wipes, and loading screens honestly
* stronger camera bootstrap/follow ownership
* more reusable gameplay-facing runtime helpers moved out of sandbox-local glue
* clearer diagnostics around scene/runtime behavior and transition state

For the system boundary this milestone should follow, see:

* [ARCHITECTURE_NOTES.md](/Users/zshrout/dev/CarrotGameEngine/docs/ARCHITECTURE_NOTES.md)
* [CARROT_MASTER_PLAN.md](/Users/zshrout/dev/CarrotGameEngine/docs/CARROT_MASTER_PLAN.md)
* [input_router_direction.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/input_router_direction.md)
* [runtime_iteration_and_tooling.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/runtime_iteration_and_tooling.md)

---

## Scope Summary

Milestone 16 is:

* a scene runtime milestone
* a gameplay-facing runtime API milestone
* a transition lifecycle milestone
* a camera/runtime ownership milestone
* a runtime polish and diagnostics milestone

Milestone 16 is not:

* a full save/load architecture milestone
* a quest/dialogue framework milestone
* a giant gameplay framework milestone
* a cinematic timeline/cutscene system milestone
* a full editor scene-authoring milestone
* a renderer rewrite milestone

The key rule is:

This milestone should make scene-driven runtime flow feel more intentional and more reusable without collapsing Carrot into an over-prescriptive gameplay framework.

---

## Core Architectural Rule

Scene transitions must be architecturally asynchronous.

This is not optional.

If Carrot exposes:

* fades
* wipes
* loading screens
* transition overlays
* transition progress state

those presentation surfaces must sit on top of a runtime transition model that can perform scene/world replacement asynchronously.

### Required Consequence

A loading screen that does not permit meaningful async scene transition work is not considered a valid milestone outcome.

This means Milestone 16 must not stop at:

* "show a fade while blocking the main thread"
* "flash an overlay while immediately replacing the world"
* "pretend there is a loading screen while nothing is asynchronously staged"

Instead, the engine should move toward a transition model with distinct phases such as:

1. transition requested
2. transition presentation begin
3. async scene/world staging
4. swap/activation point
5. transition presentation resolve
6. transition complete

Exact names can evolve, but the lifecycle must be real.

---

## Why This Milestone Comes Next

Carrot has enough subsystem foundation now that composition is the next real issue.

The engine already knows how to:

* render
* process input
* load assets
* stage worlds
* transition between scenes
* drive a player character

What is still comparatively weak is the runtime layer above those systems.

Today, good scene-driven gameplay flow is possible, but some of the best patterns still live in sandbox code:

* runtime scene state capture/application
* interaction-outcome consumption flow
* trigger-event consumption flow
* camera/update ordering expectations
* post-scene-load world setup
* runtime presentation decisions around transitions

That is exactly the kind of layer that should now be tightened.

---

## Primary Deliverables

### 1. Stronger Scene Runtime Lifecycle

Carrot should expose a clearer scene runtime lifecycle than today's mostly functional but still lean flow.

Required outcomes:

* clearer scene runtime states and transition states
* stronger `scene_runtime_context_t`-style access to current runtime information
* clearer distinction between:
  * currently active scene
  * pending transition target
  * staged/loading scene state
  * transition presentation state
* cleaner game-facing lifecycle callbacks

Important boundary:

This milestone is about runtime structure, not about over-designing a giant state machine for its own sake.

### 2. Explicit Async Scene Transition Path

Carrot should gain a real engine-owned async scene transition model.

Required outcomes:

* scene transition requests no longer imply purely synchronous world replacement
* the engine can stage scene/world work asynchronously or incrementally
* transition state is visible and queryable
* transition completion happens at an explicit safe activation point
* runtime presentation hooks can remain active while async work is in progress

Important boundary:

The engine does not need to claim "everything streams seamlessly."
It does need to provide a real async transition architecture.

### 3. Transition Presentation Hooks

Carrot should expose engine/runtime hooks that allow transition presentation to grow cleanly.

Required outcomes:

* transition begin/end hooks
* transition progress/state visibility
* a clean place for:
  * fades
  * wipes
  * loading screens
  * future transition overlays
* presentation remains layered on top of runtime truth rather than replacing it

Important boundary:

The milestone may ship only simple visual proof at first, but the lifecycle must support future polish honestly.

### 4. Camera Runtime Ownership Cleanup

Carrot should tighten ownership of scene camera startup/follow behavior.

Required outcomes:

* clearer initial camera target policy
* clearer bootstrap/follow ownership line between scene data, runtime logic, and game code
* fewer temporary presentation defaults lingering in sandbox code
* camera behavior that feels more scene-runtime-owned and less improvised

### 5. Broader Gameplay-Facing Runtime APIs

Carrot should promote a small number of recurring runtime patterns into engine-facing APIs.

Good candidates include:

* scene-runtime state handoff helpers
* reusable per-scene object state key helpers
* cleaner interaction-outcome dispatch flow
* stronger trigger/event consumption helpers
* runtime helpers for post-load world/player setup

Required rule:

Only promote patterns that are genuinely recurring and improve real game code.

### 6. Runtime Diagnostics for Scene Flow

Carrot should make scene runtime easier to inspect and debug.

Required outcomes:

* clearer logging around scene load/transition flow
* queryable transition/runtime state
* better diagnostics for:
  * current scene id
  * current spawn marker
  * pending target scene
  * transition phase
  * camera bootstrap/follow reasoning where practical

This does not need a giant debug UI, but the runtime truth should be easier to see.

---

## Required Minimum Slice

To keep this milestone sharp even though it is intentionally large, the minimum acceptable implementation should be:

1. explicit scene runtime transition state beyond today's simple load/transition flow
2. real async scene transition architecture
3. transition presentation hooks that can support loading/fade/wipe behavior honestly
4. at least one visible async transition proof
5. cleaner camera runtime ownership
6. at least one or two recurring sandbox runtime patterns promoted into engine APIs
7. better scene/runtime diagnostics and documented limitations

If this minimum slice lands cleanly, the milestone can be considered successful even if:

* full streaming-world architecture is deferred
* cinematic transition effects are still simple
* a full save system is deferred
* larger gameplay framework ideas are deferred

---

## Async Transition Rules

This milestone must settle async transition behavior clearly.

### Required Rule

Transition presentation must reflect real transition progress/state.

### Acceptable First-Pass Shapes

Any of the following may be acceptable first-pass architectures if they are honest and clean:

* background staging on a worker thread followed by a safe main-thread activation point
* incremental staged loading across frames with explicit transition state
* hybrid async asset/world preparation plus synchronized world swap

### Not Acceptable

The following are not enough:

* synchronous load hidden behind a fade
* fake loading screen with no asynchronous runtime work
* transition overlays that are architecturally detached from transition progress

### Important Boundary

Carrot does not need to promise fully streaming seamless worlds in this milestone.
It does need to stop structuring transitions as purely synchronous world replacement if it wants honest loading/presentation support.

---

## Camera Ownership Rules

This milestone should clarify camera runtime responsibility without inventing a giant camera product.

Required direction:

* scene assets can express camera-related authored policy where appropriate
* runtime scene flow owns camera bootstrap behavior
* game code should provide intentional overrides where needed
* temporary hardcoded presentation defaults should shrink

Important boundary:

This milestone should improve ownership and clarity first.
It does not need to ship a huge cinematic system.

---

## Gameplay-Facing Runtime API Rules

This milestone should improve game-facing APIs by absorbing recurring patterns that are already proving themselves in sandbox code.

The engine should increasingly expose:

* runtime intent
* scene/runtime queries
* structured transition hooks
* explicit runtime state handoff helpers

The engine should reduce the amount of game code that has to manually orchestrate:

* scene bootstrap details
* post-load state application
* transition sequencing
* interaction/trigger runtime bookkeeping

Important boundary:

This should still feel like Carrot is providing strong runtime seams, not forcing one universal gameplay framework onto every game.

---

## Ticketed Work Order

This milestone is large enough that it should be implemented through focused tickets.

### Ticket 1. Scene Runtime State Model Cleanup

Goal:

* clarify runtime state, transition state, and current scene context

Expected outcomes:

* stronger scene runtime state model
* clearer active vs pending vs staged scene data
* better scene runtime queries for game code

Why this comes first:

* later transition presentation and async work are cleaner when the runtime lifecycle is explicit

### Ticket 2. Async Transition Foundation

Goal:

* introduce a real async scene transition path

Expected outcomes:

* transition request enters structured async lifecycle
* world/scene staging no longer assumes fully synchronous swap
* explicit safe activation point

Why this comes early:

* transition presentation hooks are only honest if this exists first

### Ticket 3. Transition Presentation Lifecycle

Goal:

* add presentation-facing transition hooks on top of the async runtime path

Expected outcomes:

* transition begin/progress/resolve visibility
* simple fade/wipe/loading-screen proof
* no fake blocking-only loading screen architecture

Why this matters:

* this is where runtime and presentation finally meet cleanly

### Ticket 4. Camera Runtime Ownership Cleanup

Goal:

* move camera bootstrap/follow behavior into a cleaner engine/runtime contract

Expected outcomes:

* clearer camera startup rules
* reduced reliance on temporary sandbox defaults
* stronger scene/runtime camera ownership story

### Ticket 5. Runtime State Handoff Helpers

Goal:

* promote clean scene-transition/runtime-state helpers out of sandbox-local glue

Expected outcomes:

* cleaner capture/apply scene-runtime state patterns
* better per-scene object-state support
* easier scene-driven continuity without requiring a full save system

### Ticket 6. Interaction / Trigger Runtime API Cleanup

Goal:

* identify recurring interaction/trigger runtime flow and promote reusable pieces into engine APIs

Expected outcomes:

* cleaner interaction outcome consumption
* cleaner trigger event handling surfaces
* less custom orchestration in game state code

### Ticket 7. Scene Runtime Diagnostics

Goal:

* make runtime scene/transition truth easier to inspect

Expected outcomes:

* structured logs and queryable runtime state
* visible proof of transition phase and scene target where appropriate
* clearer debugging when scene flow goes wrong

### Ticket 8. Sandbox Proof and Closeout

Goal:

* validate the milestone through a real game-facing proof

Expected outcomes:

* async transition proof
* presentation proof that reflects real transition progress
* updated docs and explicit current limitations

---

## Suggested Work Order

1. Scene runtime state cleanup
2. Async transition foundation
3. Transition presentation lifecycle
4. Camera ownership cleanup
5. Runtime state handoff helpers
6. Interaction/trigger runtime cleanup
7. Diagnostics
8. Sandbox proof, docs, and closeout

This ordering keeps the milestone grounded in architecture first and polish second.

---

## Non-Goals

To prevent scope creep, this milestone must not:

* implement a full save/load system
* build a quest framework
* build a dialogue framework
* build a cinematic timeline system
* promise seamless open-world streaming
* turn Carrot into a giant genre-specific gameplay framework
* create editor-owned scene/runtime behavior

---

## Closeout Criteria

Milestone 16 should not be considered complete until:

* scene transitions are architecturally asynchronous
* at least one transition presentation proof reflects real async runtime progress
* scene runtime context/state is clearer and stronger than today's flow
* camera bootstrap/follow ownership is cleaner
* at least one or two good recurring sandbox runtime patterns have moved into engine APIs
* diagnostics and docs explain the resulting runtime boundaries honestly

Additional required honesty at closeout:

* if some transition paths are still synchronous, that must be explicit
* if some scene/world work still requires activation on the main thread, that must be explicit
* if loading-screen behavior is still partial or limited by current asset/world staging, that must be explicit

This milestone should close only when Carrot can truthfully say:

* scene runtime is more intentional
* transitions are async in architecture, not just in presentation
* gameplay-facing runtime APIs are stronger
* the engine gives scene-driven games less glue work and better runtime structure

