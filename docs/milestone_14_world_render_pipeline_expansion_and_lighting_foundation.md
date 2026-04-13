# Carrot Game Engine - Milestone 14

**Last Updated:** April 13, 2026
**Title:** World Render Pipeline Expansion and Lighting Foundation
**Status:** Planned
**Focus:** Move Carrot's world renderer across the architectural line into a first-pass forward+ path, then prove that path with a narrow but real 2D lighting slice built around ambient lighting and point lights.

---

## Milestone Goal

Milestone 13 intentionally stayed lean.

It improved runtime iteration, diagnostics, and tooling boundaries without letting Carrot collapse into an editor-first project.

The next major structural ceiling is back in the renderer.

Carrot now has:

* explicit frame stages
* a meaningful world render path
* stable 2D draw ordering foundations
* batching for textured quad submissions
* backend support across Vulkan, Metal, and DirectX 12

What it does not yet have is the next renderer architecture needed for the engine's longer-term identity:

* a world-lighting-aware render path
* a real light submission model
* smarter world batching/material organization beyond the current flat textured-quad assumptions
* a renderer that is clearly on the forward+ side of the architectural boundary

Milestone 14 exists to cross that boundary deliberately.

This milestone is successful if Carrot ends with:

* a world renderer that is architecturally forward+
* first useful lighting support through ambient term plus point lights
* preserved backend parity
* no dependence on editor-only behavior or UI-stage hacks

For renderer direction and intended long-term architecture, see:

* [CARROT_MASTER_PLAN.md](/Users/zshrout/dev/CarrotGameEngine/docs/CARROT_MASTER_PLAN.md)
* [ARCHITECTURE_NOTES.md](/Users/zshrout/dev/CarrotGameEngine/docs/ARCHITECTURE_NOTES.md)

---

## Scope Summary

Milestone 14 is:

* a renderer architecture milestone
* a world-pass expansion milestone
* a first-pass lighting milestone
* a batching/material submission milestone

Milestone 14 is not:

* a full final lighting milestone
* a full shadows milestone
* a post-processing milestone
* a hybrid 2D/3D milestone
* an editor milestone
* a gameplay-module milestone

The key rule is:

This milestone should make the renderer structurally ready for more serious world lighting later, without pretending all future lighting work must land immediately.

---

## Core Architectural Rule

The renderer must cross into a forward+ world architecture during this milestone.

That means:

* world rendering is no longer just "flat batched textured quads plus future plans"
* lights become engine-owned world render inputs
* the world stage may internally perform richer pass/submission work while UI/debug/composite remain clearly separate
* the engine should not need a second unrelated render architecture later just to support serious 2D lighting

This milestone does **not** need to deliver every future lighting feature.
It **does** need to ensure the renderer is honestly growing toward the intended long-term path.

---

## Primary Deliverables

### 1. World Render Pipeline Expansion

The current world stage should evolve into a more explicit internal pipeline that can support lighting-aware world rendering without forcing UI/debug/composite work into that structure.

Required outcomes:

* clearer world-render submission ownership
* explicit separation between world render data and late overlay/UI work
* a shape that can later host more lights, richer materials, and hybrid growth

### 2. Smarter World Batching / Submission Model

Carrot should move beyond the current "texture plus sampler is enough" mindset for long-term world rendering.

This does not require a huge generalized material system immediately.
It does require:

* a more deliberate batch/submission key for world draws
* continued CPU ownership of visibility, ordering, and primary world submission in the first pass
* room for lighting-related material inputs
* more GPU-friendly world instance/light payloads where the forward+ path benefits from them
* backend-safe submission behavior that remains explicit and understandable

Important boundary:

This milestone should improve batching and submission structure for a forward+ renderer, but it should **not** expand into a full GPU-driven renderer rewrite.

That means milestone 14 may include GPU-side work where forward+ clearly needs it, such as light indexing/culling or other lighting-related evaluation structure, while still keeping overall world submission ownership primarily CPU-side in the first pass.

### 3. First Lighting Slice

The first implemented lighting slice should be intentionally narrow:

* ambient lighting term
* point lights

That is enough to prove the architecture without bloating the milestone.

### 4. Engine-Owned Light Data / Runtime Path

Lights should become engine/runtime concepts, not sandbox-local renderer tricks.

The first pass may be narrow, but the engine should own:

* light data structures
* world/light submission path
* renderer-facing evaluation behavior

### 5. Backend Parity Preservation

Renderer growth in this milestone must continue to work across:

* Vulkan
* Metal
* DirectX 12

If a lighting feature only works on one backend, it is not an acceptable closeout.

---

## Required Minimum Slice

To keep the milestone sharp, the minimum acceptable implementation should be:

1. a world renderer that no longer treats lighting as purely future work
2. a smarter world submission/batching path suitable for forward+ growth
3. engine-owned ambient lighting support
4. engine-owned point light support
5. working world-lighting behavior across Vulkan, Metal, and DirectX 12
6. explicit confirmation that the first-pass renderer remains CPU-submission-driven rather than fully GPU-driven
7. documentation that makes the forward+ direction explicit

If this minimum slice lands cleanly, it is enough for milestone success even if:

* shadows are deferred
* other light types are deferred
* richer materials are still first-pass
* clustering quality or light-count scale still has later room to grow

---

## Closeout Criteria

Milestone 14 is complete when:

* the world renderer has moved to a forward+ architecture
* the old flat world path is no longer the primary renderer direction
* ambient lighting works in the engine world path
* point lights work in the engine world path
* world lighting remains separate from UI, composite, and debug stage responsibilities
* Vulkan, Metal, and DirectX 12 all support the milestone slice correctly
* the renderer/batching/material direction is documented clearly enough that later lighting and hybrid work can build on it

Important wording rule:

Completion does **not** mean Carrot has "finished lighting."
Completion **does** mean the renderer has crossed into the intended next architecture and proved it with a real first-pass lighting slice.

---

## Suggested Work Order

1. Clarify current world submission and batching limits in the renderer.
2. Refactor the world render path so it can host lighting-aware data cleanly.
3. Introduce a first engine-owned world light model and runtime submission path.
4. Land ambient lighting support first.
5. Land point lights on top of the new path.
6. Preserve and verify backend parity.
7. Update architecture docs after the renderer direction is proven in code.

This order keeps the milestone about renderer structure first and visible lighting second.

---

## Explicit Non-Goals

This milestone should **not** expand into:

* shadows
* every possible light type
* full normal-mapped material workflows unless they prove truly necessary for the first slice
* broad post-processing stack work
* a full GPU-driven batching / draw-indirect renderer rewrite
* editor-driven lighting tooling
* scene/world editing tools
* dialogue systems
* cinematics systems
* combat systems
* RPG stats systems
* full hybrid 2D/3D rendering

Those are all real future directions, but they should not hijack this renderer milestone.

---

## Design Constraints

To keep the milestone honest:

* UI, debug, and editor work must stay outside the main world-lighting architecture
* the renderer should not split into one "simple 2D" path and one "real lighting" path
* current work should favor explicit ownership and backend-safe structure over flashy one-off effects
* batching/submission improvement should stay focused on better world structure, not balloon into a full GPU-driven renderer transition
* the first-pass light model should be narrow enough to be stable, understandable, and testable
* the milestone should prefer "forward+ foundation proven cleanly" over "many lighting features implemented messily"

---

## Open Questions To Settle During Implementation

This doc intentionally leaves a few implementation questions open until the renderer work begins:

* how much material state needs to be formalized now versus deferred to a later richer material milestone
* whether the first light-authoring path should be purely runtime/code-driven or also include a small authored scene/world representation immediately
* what the practical first-pass light-count expectations should be for all three backends

These questions should be answered in code and notes without weakening the core closeout requirement:

The renderer must be forward+ by the end of the milestone.

---

## Summary

Milestone 14 is not "finish lighting."

It is:

* move the renderer into its next real architecture
* prove that architecture with ambient plus point-light support
* keep backend parity intact
* prepare Carrot for later shadows, richer lighting, and eventual hybrid 2D/3D growth without turning this milestone into an unbounded renderer rewrite
