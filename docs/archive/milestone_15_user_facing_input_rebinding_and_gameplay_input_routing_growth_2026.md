# Carrot Game Engine - Milestone 15

**Last Updated:** April 13, 2026
**Title:** User-Facing Input Rebinding and Gameplay Input Routing Growth
**Status:** Completed and archived
**Focus:** Turn Carrot's current action-map and gameplay-input foundation into a real engine-owned user-facing input system with typed runtime action identifiers, rebinding, persistence, and first-pass broader routing growth.

---

## Milestone Goal

Milestone 14 pushed the renderer across an architectural boundary.

With that work landed, Carrot now has enough engine foundation that one of the next real bottlenecks is not another hidden subsystem seam.

It is day-to-day game usability.

Carrot already has:

* engine-owned action-map parsing and binding evaluation
* gameplay input routing for the current single-player-forward sandbox flow
* controller/device support
* runtime UI foundations
* config-backed sandbox input bindings

What it does not yet have is the next input layer needed for a real engine-facing game runtime:

* engine-owned typed action identifiers instead of raw runtime string usage everywhere
* a user-facing rebinding path
* engine-owned persistence for user binding changes
* a broader routing model that can grow beyond one implicit gameplay context

Milestone 15 exists to make input feel like a real engine feature instead of a useful but still developer-lean foundation.

This milestone is successful if Carrot ends with:

* engine-owned typed action handles for runtime code
* user-facing rebinding support that does not require sandbox-local hacks
* persistent binding storage/load behavior
* preserved single-player simplicity by default
* a first clean step toward broader gameplay input routing without forcing all games into multiplayer complexity

For the system boundary this milestone should follow, see:

* [input_router_direction.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/input_router_direction.md)
* [CARROT_MASTER_PLAN.md](/Users/zshrout/dev/CarrotGameEngine/docs/CARROT_MASTER_PLAN.md)
* [ARCHITECTURE_NOTES.md](/Users/zshrout/dev/CarrotGameEngine/docs/ARCHITECTURE_NOTES.md)

---

## Scope Summary

Milestone 15 is:

* an input-systems milestone
* a user-facing rebinding milestone
* a runtime configuration milestone
* a gameplay input routing milestone

Milestone 15 is not:

* a full settings-menu milestone
* a full local-multiplayer gameplay milestone
* an online/netcode input milestone
* a per-platform controller prompt/icon milestone
* a giant UI widget expansion milestone
* a project-wide gameplay framework milestone

The key rule is:

This milestone should make input engine-owned, editable, and persistable without turning Carrot into a full input-product suite in one pass.

---

## Core Architectural Rule

Runtime input code should stop depending primarily on raw string action names.

Carrot should keep string action ids as the authored/serialized representation where they are useful, but runtime gameplay-facing code should move toward engine-owned typed identifiers.

That means:

* authored config may still say `"move_up"`
* runtime code should be able to say `input_actions.move_up`
* the engine should own the mapping between stable authored ids and typed runtime action handles
* rebinding and routing should operate on engine-owned action identity, not ad hoc string comparisons scattered through gameplay code

This milestone does **not** require one hardcoded global enum for every future game.
It **does** require a stronger runtime action identity layer than today's stringly usage.

---

## Primary Deliverables

### 1. Typed Runtime Action Identifier Layer

Carrot should introduce an engine-owned runtime action identity model.

Required outcomes:

* a typed runtime action handle or identifier type
* an engine-owned action definition/registry layer
* a clear mapping from authored string ids to typed runtime handles
* a gameplay-facing pattern closer to `profile.move_up = input_actions.move_up`

Important boundary:

This milestone should not force unreadable binary-only authored input config.
Readable string ids in config are still desirable.

### 2. Engine-Owned Binding Inspection / Mutation / Persistence

The engine should no longer treat bindings as load-only developer data.

It should support:

* inspecting active bindings
* mutating bindings at runtime
* saving bindings back out
* loading user overrides on top of defaults
* restoring defaults cleanly

This should be engine/runtime functionality, not a sandbox-only helper layer.

### 3. Rebinding Capture Flow

Carrot should gain a real runtime rebinding flow.

The first pass should support:

* entering a "listening for next input" state
* capturing the next valid key/button/axis input
* rejecting invalid or ambiguous captures where necessary
* applying the result to the engine-owned binding set
* surfacing conflicts explicitly rather than silently producing unclear behavior

This should be UI-agnostic engine logic that runtime UI can drive.

### 4. Gameplay Input Routing Growth

Carrot should take a first deliberate step beyond today's one-context input assumptions.

Required first-pass outcomes:

* preserve current single-player behavior by default
* add explicit routing policy setup
* support at least one broader routing mode beyond the current implicit path
* introduce player-facing or context-facing input state that can grow later

Recommended first routing slice:

* fixed assignment local multiplayer routing

This is the right first step because it is explicit, testable, and does not require join-flow UX immediately.

### 5. Proof Through Runtime UI or Sandbox Surface

The milestone should include a small real proof that rebinding works for a user, not just an API.

The proof may be narrow, but it should demonstrate:

* viewing current bindings
* rebinding at least one action at runtime
* saving or applying the result
* confirming that the new binding actually drives gameplay or UI behavior

---

## Required Minimum Slice

To keep the milestone sharp, the minimum acceptable implementation should be:

1. a typed runtime action identifier layer on top of authored string ids
2. engine-owned binding mutation and serialization support
3. load-defaults plus load/save-user-overrides behavior
4. engine-owned rebinding capture/session flow
5. a small runtime proof surface for rebinding
6. one explicit routing mode beyond the current implicit single-player default
7. documentation that makes the runtime action-identity direction explicit

If this minimum slice lands cleanly, it is enough for milestone success even if:

* join-to-play flow is deferred
* a richer settings menu is deferred
* prompt/icon display systems are deferred
* advanced per-device tuning UI is deferred

---

## Action Identity Rules

This milestone must settle the runtime identity model clearly.

### Required Rule

Carrot should distinguish between:

* authored action id
* runtime action handle
* optional future display metadata

### Preferred Shape

The intended runtime usage should look like:

```cpp
profile.move_up = input_actions.move_up;
profile.interact = input_actions.interact;
```

Not:

```cpp
profile.move_up = "move_up";
profile.interact = "interact";
```

### Important Boundary

This milestone should avoid painting Carrot into one giant inflexible global-enum corner.

The runtime handle may be backed by:

* a lightweight registry-owned typed id
* a generated or registered constant set
* a game/runtime action-definition table

Exact implementation can evolve, but the milestone must move runtime behavior beyond raw string identity.

---

## Persistence Rules

The rebinding system should distinguish between:

* shipped default bindings
* user override bindings
* active resolved bindings

Required behavior:

* games can ship readable default binding config
* user changes can persist without overwriting the shipped defaults
* restoring defaults is an explicit engine-supported path
* runtime binding state remains inspectable and debuggable

The milestone should prefer a narrow honest persistence model over pretending a giant settings framework already exists.

---

## Routing Rules

Broader routing growth should remain opt-in.

Default behavior:

* current single-player-forward simplicity remains intact

First expanded behavior:

* fixed-assignment local multiplayer routing is now implemented as the first opt-in broader routing mode

### Current Closeout Boundary

This milestone intentionally stops at a first honest routing foundation rather than pretending full local-multiplayer gameplay/UI ownership is solved.

Current implemented boundary:

* player 0 remains the primary UI/navigation/debug-input context
* the sandbox proof uses fixed assignment rather than join-flow
* rebinding edits the shared engine-owned binding template, so changing an action binding affects all routed player contexts that use that action map
* routing currently creates per-player runtime input contexts on top of shared bindings, not fully divergent per-player binding profiles
* fixed assignment is proven in the sandbox, but broader multi-character gameplay ownership is still future work

This is acceptable for milestone closeout because the milestone goal was broader routing growth and engine-owned rebinding/persistence, not full multiplayer gameplay parity.

---

## Closeout Summary

Milestone 15 delivered:

* typed runtime action handles on top of authored string ids
* engine-owned binding mutation, inspection, and JSON round-trip serialization
* load-defaults plus persisted user-binding behavior
* engine-owned rebind capture flow
* action catalog metadata for engine-facing inspection and future UI
* explicit routing policy setup with `single_player_auto` and `local_multiplayer_fixed`
* per-player runtime input contexts for the first broader routing slice
* sandbox proof of runtime rebinding persistence and fixed assignment routing

Key proof points:

* rebinding survives application restart
* defaults can be restored cleanly
* fixed routing can expose two logical player contexts with explicit keyboard/gamepad assignment

Archive note:

* milestone document archived after closeout on April 13, 2026

* explicit routing mode configuration
* at least one fixed-assignment broader routing path

This milestone should not force every game into player-slot complexity if it does not ask for it.

---

## Implementation Boundaries

Milestone 15 should not introduce:

* online or rollback input architecture
* fully generalized player-join UX
* per-platform icon/prompt databases
* a giant settings framework unrelated to input
* broad UI-design churn just to host rebinding
* speculative support for every future control device class

The purpose here is to make Carrot's input system real and shippable, not to solve the entire future of input UX in one pass.

---

## Suggested Work Order

1. Introduce the runtime action identifier / definition layer.
2. Refactor the current gameplay input profile to use typed action handles.
3. Expand the action-map/binding layer to support inspection, mutation, and serialization.
4. Add engine-owned default-vs-user binding persistence behavior.
5. Add rebinding capture/session support.
6. Add a narrow runtime proof surface for rebinding.
7. Add fixed-assignment broader routing support.
8. Update docs once the runtime input identity model is proven in code.

This order keeps the milestone about input architecture first and UI proof second.

---

## Explicit Non-Goals

This milestone should **not** expand into:

* online multiplayer
* rollback netcode preparation
* split-screen gameplay systems
* per-platform input glyph packs
* accessibility remapping for every possible device edge case
* giant menu/theme work
* controller vibration/rumble framework growth
* save-game/profile management unrelated to input settings

Those are all valid future directions, but they should not hijack this milestone.

---

## Design Constraints

To keep the milestone honest:

* runtime input should become less stringly, not more
* authored config should stay readable
* single-player ergonomics should remain first-class
* broader routing should stay opt-in and explicit
* rebinding logic should be engine-owned rather than buried in sandbox UI code
* the milestone should prefer "typed input identity and user rebinding proven cleanly" over "huge settings UX implemented messily"

---

## Open Questions To Settle During Implementation

This doc intentionally leaves a few implementation questions open until the work begins:

* whether the typed action handle should be registry-backed, generated, or another lightweight engine-owned form
* whether user overrides should serialize as a full resolved binding file or a smaller override-only file
* how aggressive first-pass conflict resolution should be during rebinding
* whether the first routing expansion should stop at fixed assignment or include a small join-flow proof if it comes cheaply

These questions should be answered in code and notes without weakening the core closeout requirement:

Carrot must end the milestone with engine-owned typed runtime action identity and real user-facing rebinding support.

---

## Summary

Milestone 15 is not "finish all input."

It is:

* move runtime input identity beyond raw strings
* make rebinding a real engine-owned feature
* keep single-player simplicity intact
* open the door to broader routing growth without forcing complexity on every game
