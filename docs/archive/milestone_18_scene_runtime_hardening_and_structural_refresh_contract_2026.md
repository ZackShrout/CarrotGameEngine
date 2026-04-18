# Carrot Game Engine - Milestone 18

**Last Updated:** April 17, 2026
**Title:** Scene Runtime Hardening and Structural Refresh Contract
**Status:** Completed and archived
**Focus:** Turn Carrot's already-real scene/runtime layer into a more permanent engine contract by hardening lifecycle ownership, refresh/rebuild behavior, and structural asset handling without slipping into editor-owned or sandbox-owned scene authority.

Archived in April 2026 after Carrot hardened scene-runtime ownership, structural refresh and rebuild diagnostics, post-activation runtime binding, and moved authored scene lighting into the engine-owned scene load path instead of sandbox-local gameplay bootstrap code.

---

## Milestone Goal

Milestones 16 and 17 gave Carrot a real scene/runtime layer:

* scene loading and transition lifecycle
* staged and asynchronous scene preparation
* engine-owned runtime transition presentation
* runtime scene and object inspection surfaces
* scene rebuild support
* more honest reload/rebuild policies for structural assets

That means the next question is no longer:

* can Carrot load authored scenes?
* can Carrot transition between scenes?
* can Carrot inspect active scene/runtime state?

It can.

The next question is:

* whether the scene/runtime layer is now permanent enough to build on confidently
* whether rebuild and refresh behavior is explicit instead of scattered across subsystems
* whether structure-shaping assets have a clear runtime contract
* whether scene runtime responsibilities are staying cleanly separated from renderer, tooling, and game-specific glue

Milestone 18 exists to make scene runtime a stronger engine boundary instead of merely a successful feature stack.

This milestone is successful if Carrot ends with:

* a clearer contract for what scene runtime owns
* explicit structural refresh rules for scenes, tilemaps, and other world-shaping assets
* stronger scene rebuild behavior and diagnostics
* cleaner runtime binding between scene, world, camera, music, and gameplay-facing controllers
* fewer hidden assumptions living only in sandbox flow

Meaningful milestone progress is already landed:

* scene-runtime listener and activation ordering are more explicit
* rebuild requests and outcomes are more visible in diagnostics
* structural refresh policy is surfaced more honestly in runtime/editor tooling
* authored scene lighting now imports through engine scene load instead of sandbox gameplay bootstrap code

Closeout assessment:

* the milestone minimum slice is satisfied
* the main remaining work is final closeout judgment and any small opportunistic cleanup, not another major architecture push

---

## Scope Summary

Milestone 18 is:

* a scene/runtime hardening milestone
* a lifecycle ownership milestone
* a rebuild and structural refresh milestone
* a gameplay-facing runtime contract milestone
* a diagnostics and trustworthiness milestone

Milestone 18 is not:

* a scene editing milestone
* a prefab/archetype authoring milestone
* a save/load architecture milestone
* a quest/dialogue framework milestone
* a renderer rewrite milestone
* a generalized ECS conversion milestone

The key rule is:

**Scene runtime should become easier to trust, easier to reason about, and harder to misuse.**

---

## Why This Milestone Comes Next

Carrot's scene/runtime layer is already doing real work.

That is exactly why it needs hardening now.

Current strengths:

* scene runtime state and transition phases exist
* scene rebuild paths already exist
* runtime summary and inspection surfaces already exist
* camera bootstrap/follow behavior is scene-aware
* scene-owned music bootstrap exists
* authored scene/world lighting now has a clear engine-owned contract instead of staying sandbox-local runtime glue
* interaction and trigger flow can already drive scene transitions and authored outcomes

Current risks:

* multiple systems can influence scene/runtime behavior without one explicit refresh contract
* rebuild-required assets are now visible, but the full runtime behavior around them is still spread across scene, asset, and sandbox expectations
* the scene/runtime layer is at risk of becoming a catch-all coordinator if its boundaries are not clarified now
* successful sandbox patterns may silently become engine rules before they are formalized deliberately

This is exactly the right time to harden the contract while the shape is already real but not yet overgrown.

Recent proof point:

* `CarrotEditor` and sandbox gameplay now report the same authored town lighting because both read the same engine-owned scene/world lighting state

---

## Core Architectural Rule

Structural refresh must be an explicit engine concept.

Some assets merely update runtime data.
Some assets reshape the loaded world.
Those are not the same thing and the engine should stop treating them as if they differ only by convenience level.

Required consequence:

* scene runtime should know when "reload" really means "rebuild current scene"
* systems should not guess ad hoc whether existing world state is still structurally valid
* diagnostics should make structural refresh requirements visible
* games should not need sandbox-local glue to decide how to recover from scene-shaping asset changes
* authored scene data such as lighting should flow through engine-owned import paths instead of gameplay-state bootstrap code

If the runtime can only recover correctly because sandbox code knows a special case, that contract is not hardened enough for milestone closeout.

Current assessment:

* the most important previously hidden sandbox-specific scene/runtime special case was authored lighting
* that case now lives in the engine-owned scene load path and shows up truthfully in both sandbox gameplay and `CarrotEditor`

---

## Primary Deliverables

### 1. Scene Runtime Ownership Audit and Cleanup

The engine should clarify exactly what `scene_runtime_t` owns versus what it merely coordinates.

Required outcomes:

* explicit ownership expectations for:
  * active scene identity
  * pending transition identity
  * staged/rebuilding world state
  * camera defaults and applied overrides
  * scene-owned music bootstrap
  * controller rebinding to rebuilt world objects
* reduced ambiguity around which layer is allowed to mutate what during transition and rebuild flow

Important boundary:

This milestone should simplify the contract, not inflate it into a giant framework.

### 2. Structural Refresh Contract

Carrot should formalize which asset/runtime changes require:

* live reload
* next-use refresh
* current-scene rebuild
* full restart or broader runtime reset

Required outcomes:

* a clearly documented and engine-visible structural refresh contract
* stronger mapping between structural asset kinds and rebuild behavior
* scene runtime helpers that can safely rebuild current scene without sandbox-specific recovery logic
* explicit handling for scene, tilemap, and font cases where structural or presentation assumptions may no longer hold

### 3. Stronger Rebuild Current Scene Path

The existing rebuild path should become a first-class runtime behavior rather than a convenience edge path.

Required outcomes:

* rebuild requests are visible, deterministic, and diagnosable
* rebuild uses current scene id and effective spawn context intentionally
* controller/runtime reattachment after rebuild is clear and testable
* scene-owned state restoration rules are explicit where applicable
* failures leave the runtime in a safe and understandable state

### 4. Cleaner Runtime Binding Surfaces

Carrot should reduce accidental coupling between scene runtime and game-side setup logic.

Required outcomes:

* cleaner expectations around:
  * player controller binding
  * interaction controller binding
  * camera follow target selection
  * scene music application
  * post-load runtime validation
  * scene-authored lighting import and runtime follow binding
* fewer hidden order dependencies between scene activation and gameplay-facing runtime code

### 5. Stronger Diagnostics Around Scene Refresh and Rebuild

Rebuild behavior should be easy to explain after the fact.

Required outcomes:

* logs and structured summaries explain:
  * what triggered a rebuild
  * which scene is being rebuilt
  * what phase failed if rebuild does not complete
  * whether the previous active world remained intact
* runtime summaries remain truthful during rebuild and transition phases

---

## Ticket Breakdown

### Ticket 18.1 - Scene Runtime Responsibility Audit

Review and tighten the responsibility boundary between:

* `scene_runtime_t`
* `scene_load_task_t`
* world/state continuity helpers
* controller/runtime binding helpers
* game-facing listener hooks

Deliverables:

* code cleanup where responsibilities are currently blurred
* clearer inline documentation/comments where the ownership line is subtle
* doc updates for the permanent contract

### Ticket 18.2 - Structural Refresh Policy Surface

Define and expose the runtime meaning of structural refresh more clearly.

Deliverables:

* clear relationship between asset reload policy and rebuild policy
* scene/runtime helper API for rebuild-required situations
* stronger mapping from asset diagnostics to runtime action

### Ticket 18.3 - Rebuild Current Scene Hardening

Turn rebuild into a fully trustworthy engine path.

Deliverables:

* stronger rebuild request validation
* stronger activation/failure handling
* more robust reuse of current scene/spawn/runtime context
* expanded tests for success and failure cases

### Ticket 18.4 - Runtime Binding and Post-Activation Cleanup

Audit all engine-side work that happens after a scene becomes active.

Deliverables:

* clearer order for world activation, controller binding, camera application, music application, and listener callbacks
* removal of avoidable sandbox-local assumptions
* stronger runtime summary truth after activation and rebuild

### Ticket 18.5 - Scene Rebuild Diagnostics and Inspection

Improve visibility into rebuild and refresh flow.

Deliverables:

* stronger logs
* stronger structured scene summary data where needed
* editor/runtime inspection surfaces that reflect rebuild state honestly

### Ticket 18.6 - Regression Coverage

Add or expand tests covering:

* rebuild success
* rebuild failure
* rebuild while preserving current active scene on failure
* controller rebinding after rebuild
* camera/music/runtime context correctness after rebuild

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. a clearer engine contract for structural refresh versus live reload
2. a hardened `rebuild current scene` path that is treated as a real runtime workflow
3. clearer runtime ownership between scene, world, controller, camera, and music layers
4. stronger diagnostics around rebuild requests and outcomes
5. tests proving rebuild behavior is trustworthy in both success and failure paths

If these land cleanly, the milestone succeeds even if:

* some future scene-system abstractions still remain for later work
* save/persistence-aware rebuild behavior is deferred
* richer scene authoring workflows are still future work

---

## Closeout Criteria

Milestone 18 is complete when:

* scene runtime responsibilities are clearer and better bounded
* structural refresh is an explicit engine concept rather than an implicit special case
* rebuild-required assets map cleanly onto runtime rebuild behavior
* rebuilding the current scene is a trustworthy and diagnosable engine path
* current runtime inspection surfaces remain truthful across rebuild phases
* sandbox does not need hidden glue to make rebuild flow feel correct

Completion does not mean scene runtime is "finished forever."
It does mean the current architecture is durable enough that later gameplay/runtime growth can build on it without dragging along ambiguous refresh behavior.
