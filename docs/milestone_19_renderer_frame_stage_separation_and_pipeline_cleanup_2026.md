# Carrot Game Engine - Milestone 19

**Last Updated:** April 15, 2026
**Title:** Renderer Frame Stage Separation and Pipeline Cleanup
**Status:** Proposed
**Focus:** Strengthen Carrot's renderer as a permanent engine architecture by clarifying frame-stage boundaries, separating world/UI/composite/debug responsibilities more deliberately, and reducing central renderer coordination pressure before more rendering features land.

---

## Milestone Goal

Milestones 03, 05, 11, 14, and 17 moved Carrot's renderer meaningfully forward:

* explicit frame stages exist
* world rendering is real
* 2D layering and visibility behavior are real
* runtime text rendering is real
* forward+ world lighting exists
* multi-window presentation exists
* UI and debug/log rendering already share real engine paths

That means the next renderer bottleneck is no longer:

* can Carrot render scenes, UI, text, and overlays at all?

It can.

The next renderer bottleneck is:

* whether frame-stage boundaries are strong enough for long-term growth
* whether the renderer is still carrying too much policy in one central object
* whether later features such as richer composite work, transitions, tooling overlays, and broader materials can land without turning the renderer into an everything-bucket

Milestone 19 exists to make the renderer easier to extend permanently, not just easier to patch.

This milestone is successful if Carrot ends with:

* cleaner separation between world, UI, composite, debug, and log-console responsibilities
* clearer renderer execution contracts per frame stage
* reduced coupling between submission, batching, and stage execution logic
* preserved existing behavior across Vulkan, Metal, and DirectX 12
* a renderer that is easier to evolve without central-class sprawl

---

## Scope Summary

Milestone 19 is:

* a renderer architecture cleanup milestone
* a frame-stage contract milestone
* a submission and execution separation milestone
* a future-proofing milestone for rendering growth

Milestone 19 is not:

* a giant material-system milestone
* a shadows milestone
* a post-processing showcase milestone
* a hybrid 2D/3D rendering milestone
* a GPU-driven renderer rewrite milestone
* a UI redesign milestone

The key rule is:

**Rendering growth should land on stronger stage boundaries, not on a larger central renderer blob.**

---

## Why This Milestone Comes Next

The renderer already has enough responsibility that the shape now matters as much as the features.

Current strengths:

* real world rendering with lighting
* explicit frame stage concepts
* textured quad and text quad paths across backends
* world/UI/debug/log-console work already coexist
* multi-window presentation is real

Current risks:

* `renderer_t` remains a central coordinator for many concerns at once
* world and non-world work are distinct conceptually but still too close mechanically
* future transition/composite/presentation work could sprawl into the same wide execution path
* backend parity becomes harder to preserve when stage contracts are not explicit enough

Before more rendering features are added, the renderer should be made easier to reason about.

---

## Core Architectural Rule

Frame stages must be treated as real contracts, not just convenient labels.

That means:

* each stage should have clearer ownership of coordinate space, submission expectations, and execution timing
* world rendering should remain structurally distinct from UI/debug/composite work
* stage-specific work should not require one increasingly giant render path to know every policy detail

If a later feature can only be added by teaching one central renderer path yet another special-case execution rule, milestone 19 has not gone far enough.

---

## Primary Deliverables

### 1. Stronger Frame Stage Contract

Carrot should formalize what each frame stage means operationally.

Required outcomes:

* clearer execution expectations for:
  * `world`
  * `ui`
  * `composite`
  * `overlay_debug`
  * `log_console`
* cleaner documentation of stage spaces and responsibilities
* fewer implicit assumptions that one stage may safely borrow from another

### 2. Submission and Execution Separation Cleanup

The renderer should reduce accidental coupling between:

* draw submission
* world batching
* stage batching
* stage execution
* backend recording

Required outcomes:

* clearer separation between stage-local submission state and execution behavior
* fewer mixed responsibilities inside shared renderer internals
* easier future extraction of specialized stage behavior where appropriate

### 3. World vs Non-World Rendering Boundary Hardening

World rendering is already richer than other stages and should be treated accordingly.

Required outcomes:

* world stage remains the home for lighting-aware world rendering
* UI/debug/log/composite stages remain simpler and more predictable
* non-world stages do not accumulate world-render assumptions accidentally

### 4. Composite / Presentation Growth Readiness

Carrot does not need a large new composite feature set in this milestone.
It does need a renderer shape that makes such work safer later.

Required outcomes:

* composite-stage expectations are explicit
* screen-space presentation effects have a cleaner future home
* transition overlays, fades, and later post-world effects do not need to smuggle themselves through the wrong stage

### 5. Regression Safety and Backend Preservation

Cleanup work must preserve current engine capability.

Required outcomes:

* world rendering still behaves correctly
* UI/debug/log rendering still behaves correctly
* text rendering still behaves correctly
* backend behavior remains aligned across Vulkan, Metal, and DirectX 12 for the milestone slice

---

## Ticket Breakdown

### Ticket 19.1 - Frame Stage Meaning Audit

Audit current frame-stage behavior and turn any implicit expectations into explicit contracts.

Deliverables:

* clarified per-stage docs
* cleanup where stage behavior is too implicit or mixed
* comments or helper abstractions where stage-space assumptions are subtle

### Ticket 19.2 - Renderer Responsibility Decomposition

Reduce central renderer coordination pressure without rewriting everything.

Deliverables:

* identify and extract the most obvious mixed-responsibility areas
* separate stage planning, submission, and execution more clearly
* keep the resulting architecture boring and understandable

### Ticket 19.3 - World Stage Boundary Cleanup

Harden the world stage as the home for lighting-aware world rendering.

Deliverables:

* cleaner internal separation for world submission/execution data
* reduced bleed of world-specific assumptions into non-world stage logic
* safer future footing for richer world materials/effects

### Ticket 19.4 - Non-World Stage Cleanup

Treat UI, composite, overlay debug, and log console as deliberate engine stages, not leftovers.

Deliverables:

* cleaner submission expectations
* simpler stage-local rules
* clearer relationship to viewport/render-target spaces

### Ticket 19.5 - Composite and Transition Readiness

Lay groundwork for later full-screen presentation work without overbuilding it now.

Deliverables:

* clear composite-stage execution expectation
* cleaner home for later screen overlays, fades, flashes, tinting, and similar work

### Ticket 19.6 - Regression Coverage and Validation

Add or expand tests and validation where practical.

Deliverables:

* stronger behavioral checks around stage ordering and stage-specific output assumptions
* backend sanity validation for milestone-critical stage behavior

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. a clearer and documented contract for frame-stage behavior
2. reduced mixing between submission and execution responsibilities
3. a stronger architectural line between world and non-world rendering
4. preserved current renderer behavior across the supported backends for the milestone slice
5. a renderer shape that makes later composite and presentation growth cleaner

If those land cleanly, the milestone succeeds even if:

* a broader material-system refactor is deferred
* richer composite effects are still later work
* full GPU-driven submission remains a future milestone

---

## Closeout Criteria

Milestone 19 is complete when:

* frame stages are more explicit and better bounded
* world/UI/composite/debug/log responsibilities are clearer
* `renderer_t` is carrying less mixed coordination responsibility
* current rendering capability remains intact
* the engine has a cleaner renderer architecture for future growth

Completion does not mean the renderer is final.
It does mean future rendering work can land on clearer architecture instead of continuing to widen the same coordination surface.
