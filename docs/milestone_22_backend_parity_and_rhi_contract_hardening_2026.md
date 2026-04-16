# Carrot Game Engine - Milestone 22

**Last Updated:** April 15, 2026
**Title:** Backend Parity and RHI Contract Hardening
**Status:** Proposed
**Focus:** Preserve Carrot's native multi-backend identity by hardening the practical RHI contract, reducing uneven or stale backend surfaces, and making parity a more durable architectural property before more renderer features expand the gap.

---

## Milestone Goal

Carrot already has something valuable that is easy to lose:

* Vulkan is real
* Metal is real
* DirectX 12 is real
* the engine already renders meaningful content across them
* shared shader and renderer architecture already spans them

That means the next backend question is no longer:

* can Carrot technically claim multiple graphics backends?

It can.

The next backend question is:

* whether parity is being preserved intentionally or merely surviving for now
* whether the practical RHI contract is clear enough to support continued renderer growth
* whether stale or uneven backend surfaces will become drag as more renderer features land

Milestone 22 exists to make backend parity a stronger engine property instead of a fragile achievement.

This milestone is successful if Carrot ends with:

* a clearer minimum RHI contract
* less ambiguity about which backend surfaces are real versus legacy or stub-shaped
* stronger parity discipline across Vulkan, Metal, and DirectX 12
* better validation that new renderer work is not silently becoming one-backend-first forever

---

## Scope Summary

Milestone 22 is:

* an RHI contract milestone
* a backend parity milestone
* a renderer-foundation preservation milestone
* a validation and cleanup milestone

Milestone 22 is not:

* a new rendering-feature milestone
* a fourth graphics-backend milestone
* a full cross-platform tooling/install milestone
* a compiler-support-expansion milestone
* a marketing parity milestone disconnected from actual behavior

The key rule is:

**Parity should be expressed through honest contracts and maintained behavior, not just through aspirational labels.**

---

## Why This Milestone Comes Next

By milestone 22, the renderer and runtime layers above the RHI should be stronger and cleaner.
That is exactly when the backend contract needs hardening before new features widen the gap again.

Current strengths:

* all three target backends participate in meaningful rendering paths
* shared shader pipeline direction already exists
* text rendering and world rendering already span multiple backends
* multi-window and stage-based rendering work already pressure the backends in real ways

Current risks:

* some lower-level surfaces are not equally mature
* some older or broader device-style methods may no longer reflect the practical execution path honestly
* later renderer features will compound parity debt if contract cleanup does not happen now
* parity can be "mostly true" while still getting more expensive to preserve each milestone

This is the right time to tighten the foundation before the next wave of renderer expansion.

---

## Core Architectural Rule

The RHI contract should describe the real engine path, not a larger imaginary abstraction surface.

That means:

* the practical minimum backend contract should be explicit
* stale, misleading, or underused abstraction paths should be cleaned up, narrowed, or clearly isolated
* parity claims should map to tested or at least deliberately validated milestone slices

If a backend feature only exists on paper inside the abstraction while the real engine path uses a different shape, milestone 22 has not gone far enough.

---

## Primary Deliverables

### 1. Practical RHI Contract Audit

Review the currently exercised RHI path and determine what the true minimum contract is.

Required outcomes:

* clearer understanding of which RHI surfaces are actively used by the engine
* identification of stale, misleading, or legacy-shaped methods
* cleanup or narrowing where the abstraction is broader than the real engine path needs today

### 2. Backend Surface Honesty Cleanup

Carrot should reduce the gap between:

* the contract the engine appears to expose
* the contract the engine actually relies on

Required outcomes:

* fewer ambiguous or misleading backend surfaces
* better isolation of not-yet-generalized paths
* more honest backend capability expectations

### 3. Parity Validation for the Current Renderer Slice

Parity should be preserved for the renderer capabilities Carrot currently treats as real.

Required outcomes:

* clearer parity expectations for:
  * world rendering
  * text rendering
  * UI/debug/log rendering
  * multi-window presentation where in current scope
* stronger validation or milestone closeout checks for the supported slice

### 4. Shared Limits and Backend Behavior Discipline

As renderer features expand, shared limits and backend assumptions should remain deliberate.

Required outcomes:

* clearer handling of shared renderer/RHI limits where they affect backend behavior
* fewer hidden per-backend assumptions
* better visibility into parity-sensitive constraints

### 5. Documentation and Engineering Discipline

The engine should document what parity means in practice at this stage.

Required outcomes:

* clearer docs around current backend support and limitations
* clearer engineering expectation that new renderer work must preserve parity for the supported slice

---

## Ticket Breakdown

### Ticket 22.1 - RHI Usage Surface Audit

Audit which RHI methods and backend paths are truly part of the live engine path.

Deliverables:

* map of practically used RHI contract surfaces
* identification of stale, misleading, or underexercised areas

### Ticket 22.2 - Backend Contract Cleanup

Clean up the most problematic mismatches between abstraction and reality.

Deliverables:

* narrowed or clarified surfaces where appropriate
* clearer backend expectations in code and docs

### Ticket 22.3 - Renderer Slice Parity Validation

Make parity a more explicit engineering standard for the current engine slice.

Deliverables:

* validation checklist or regression support for milestone-relevant behavior
* clearer expectations for world/text/UI/debug/log paths across the three backends

### Ticket 22.4 - Shared Constraint and Limit Review

Review shared renderer/backend assumptions that affect parity.

Deliverables:

* clearer ownership of shared limits and assumptions
* better visibility where backend behavior depends on those limits

### Ticket 22.5 - Documentation and Closeout Discipline

Update docs so they describe backend support honestly.

Deliverables:

* current-state documentation on practical backend support
* clearer wording around parity scope and current limitations

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. a clearer practical RHI contract
2. cleanup of the most misleading backend abstraction mismatches
3. stronger parity expectations for the renderer slice Carrot currently treats as real
4. documentation that states support and limitations more honestly

If these land cleanly, the milestone succeeds even if:

* broader future renderer work still remains
* some backend depth work is intentionally deferred past the current engine slice

---

## Closeout Criteria

Milestone 22 is complete when:

* the practical RHI contract is clearer
* backend abstraction surfaces are more honest
* parity across Vulkan, Metal, and DirectX 12 is better preserved for the current supported slice
* docs and engineering expectations describe parity in a truthful, maintainable way

Completion does not mean all backend work is finished.
It does mean Carrot is less likely to lose its multi-backend identity through quiet architectural drift.
