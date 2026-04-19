# Carrot Game Engine - Milestone 24

**Last Updated:** April 19, 2026
**Title:** GPU Forward+ Light Classification
**Status:** Implemented, pending DirectX 12 runtime validation
**Focus:** Move world forward+ tile/light classification from CPU work to GPU compute while preserving current lit world behavior, shared limit ownership, and backend parity.

---

## Milestone Goal

Carrot already crossed into a real forward+ world-render architecture in milestone 14.
That is valuable progress.

The milestone started from a renderer where the actual tile/light list build was still CPU-owned:

* world point lights were gathered on CPU
* tile coverage was evaluated on CPU
* the tile/light index list was built on CPU
* the result was uploaded for shader consumption

That means the next forward+ question is no longer:

* does Carrot have a real world-lighting-aware renderer?

It does.

The next forward+ question is:

* whether the forward+ structure is GPU-driven where it matters
* whether the renderer can stop paying CPU cost for work that maps naturally to compute
* whether the forward+ path is becoming the first real proof of the compute-capable contract from milestone 23

Milestone 24 exists to make forward+ classification a GPU job instead of a CPU burden.

This milestone is successful if Carrot ends with:

* point lights uploaded in a GPU-friendly data model
* compute-built tile headers and light-index lists
* preserved world textured-quad lighting behavior
* retained overflow/debug visibility for renderer truth
* shared forward+ limits still owned explicitly across C++ and shader builds

---

## Scope Summary

Milestone 24 is:

* a forward+ implementation milestone
* a compute-integration milestone
* a renderer-truth preservation milestone
* a parity-minded renderer expansion milestone

Milestone 24 is not:

* full GPU-driven world submission
* tilemap culling
* bloom/post processing
* a broader lighting-model rewrite
* a shadows milestone

The key rule is:

**Forward+ should become GPU-classified before Carrot tries to move broader world submission onto the GPU.**

---

## Why This Milestone Comes Next

Forward+ is the cleanest first real GPU job in this codebase because:

* the current CPU implementation is explicit and isolated
* the current world-stage data flow already exposes the light and tile concepts clearly
* shared limit coordination already exists between C++ and HLSL through generated config
* the fragment shader already consumes a tile/light list model that can remain conceptually stable

This makes milestone 24 the best proof milestone for the new compute-capable contract:

* it exercises compute dispatch
* it exercises GPU-written structured data
* it exercises later-pass GPU consumption of that data
* it does so without forcing a full world-submission rewrite immediately

---

## Core Architectural Rule

The engine should preserve the conceptual forward+ model while changing where the classification work happens.

That means:

* lights remain engine-owned world inputs
* tile-space meaning remains renderer-owned and explicit
* shared limits remain parity-sensitive contract inputs
* the GPU should build the tile/light index structures for the live world slice

If the milestone changes the implementation but makes the data flow less understandable or less debuggable, it has not improved the architecture enough.

---

## Primary Deliverables

### 1. GPU-Friendly Forward+ Data Model

Carrot should split CPU-owned inputs from GPU-built outputs more cleanly.

Required outcomes:

* point-light data uploads in a GPU-friendly structured form
* tile/light output buffers that are written by compute rather than CPU loops
* less dependence on a giant CPU-authored per-stage payload

### 2. Compute Tile Classification Pass

Forward+ tile classification should become an explicit compute pass.

Required outcomes:

* compute classification of lights against visible world tiles
* GPU-built tile headers
* GPU-built packed light-index list

### 3. World Stage Integration

The world stage should consume the GPU-built forward+ results without changing its larger stage identity.

Required outcomes:

* preserved lit world rendering
* preserved world-only lighting-aware stage behavior
* no bleed of forward+ assumptions into non-world stages

### 4. Shared Limit Hardening

The current shared forward+ limit ownership is already valuable.
This milestone should preserve and clarify it.

Required outcomes:

* shared forward+ limits remain coordinated across CMake, C++, and shader builds
* backend provisioning continues to reflect those limits
* changes remain treated as parity-sensitive contract changes

### 5. Diagnostics and Validation

Carrot should preserve the current discipline of making renderer constraints visible.

Required outcomes:

* dropped light overflow remains visible
* dropped tile/light references remain visible
* milestone docs and validation notes explain the new GPU path honestly

---

## Ticket Breakdown

### Ticket 24.1 - Forward+ Data Model Refactor

**Priority:** P0
**Outcome:** The renderer and shaders use a GPU-friendly forward+ data layout instead of a purely CPU-authored payload model.

#### Why

The current implementation is explicit and workable, but it assumes CPU ownership of tile/light list generation.
That assumption needs to be removed before compute classification can be honest.

#### Scope

Refactor the forward+ data model so that:

* CPU owns shared config and per-frame light inputs
* GPU owns tile-header and light-index output generation
* shader consumption remains understandable and close to the current conceptual model

#### Acceptance Criteria

* forward+ input/output ownership is clearly split
* shared config ownership remains explicit across code and shaders
* the resulting data model is backend-safe for the milestone slice

### Ticket 24.2 - GPU Point-Light Upload and Buffer Provisioning

**Priority:** P0
**Outcome:** World point lights are uploaded through a GPU-friendly storage/structured buffer path.

#### Scope

Replace the old CPU-shaped assumption that all dynamic forward+ data lives only inside a giant uploaded stage payload.

#### Acceptance Criteria

* point-light upload is storage/structured-buffer friendly
* native backends provision the needed resources honestly
* light-count overflow remains visible in renderer stats/logging

### Ticket 24.3 - Compute Tile Classification Pass

**Priority:** P0
**Outcome:** Compute builds tile headers and light-index lists for the visible world slice.

#### Scope

Add a compute pass that:

* evaluates light overlap against visible world tiles
* writes tile-header metadata
* writes packed light-index lists
* respects the current shared forward+ limits

#### Acceptance Criteria

* GPU-generated tile/light data drives the forward+ world slice
* tile-space meaning remains aligned with the renderer's world-camera behavior
* overflow handling is explicit and observable

### Ticket 24.4 - World Stage Integration and CPU Path Retirement

**Priority:** P1
**Outcome:** The live world stage consumes GPU-generated forward+ results instead of CPU-built lists.

#### Scope

Integrate the compute path into the world stage while preserving:

* world-only lighting awareness
* current world-stage execution boundaries
* non-world unlit stage behavior

#### Acceptance Criteria

* lit world rendering uses GPU-built forward+ data in the milestone slice
* the previous CPU nested tile/light classification loop is retired or relegated to narrow fallback/debug use only
* the world stage remains the only lighting-aware stage

#### Current Status

Implemented on April 19, 2026 for the current milestone slice:

* the live world stage dispatches compute classification before graphics
* textured-quad world lighting reads GPU-built forward+ buffers instead of CPU-built tile/light lists
* the previous CPU nested tile/light classification loop is retired from the live world path

### Ticket 24.5 - Forward+ Diagnostics and Validation

**Priority:** P1
**Outcome:** Carrot keeps forward+ constraints visible after the implementation shift.

#### Scope

Preserve and update:

* renderer stats for world light counts
* dropped-overflow visibility
* parity docs and validation notes for the GPU forward+ slice

#### Acceptance Criteria

* forward+ diagnostics remain truthful after the move
* docs describe the new GPU-owned classification path honestly
* milestone-closeout validation includes Vulkan, Metal, and DirectX 12 expectations

#### Current Status

Implemented on April 19, 2026 for the current milestone slice:

* renderer stats/debug output still reports world light counts and dropped light overflow after the move to GPU classification
* forward+ tile/light-reference diagnostics are recalculated through a narrow CPU-side mirror of the compute overlap math, without reviving the retired live CPU classification path
* with the current shared caps, `forward_plus_dropped_light_references` is expected to remain `0` in normal operation because the total world-light cap matches the current per-tile light-index budget
* Vulkan and Metal were manually validated against the live GPU forward+ path on April 19, 2026
* DirectX 12 shared-code compilation is current, while native Windows runtime validation is still pending follow-up
* the current DirectX 12 follow-up surfaced an important contract clarification: `cpu_writable` storage buffers are a logical renderer/RHI contract, not a guarantee that the final GPU-visible storage resource is directly host-mapped on every backend
* on DirectX 12, the current milestone slice now satisfies CPU-authored forward+ storage uploads through an upload-backed shadow resource that is flushed into the real default-heap UAV buffer before compute dispatch; this is acceptable backend adaptation for milestone 24, but the contract wording should remain documented clearly until a later RHI cleanup makes CPU upload vs GPU storage intent more explicit

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. GPU-friendly point-light upload
2. compute-built tile headers and light-index lists
3. world-stage consumption of those GPU-built results
4. preserved lit world rendering behavior for the milestone slice
5. retained diagnostic visibility and shared-limit discipline

If those land cleanly, the milestone succeeds even if:

* more advanced light types are still later work
* full GPU-driven world submission is still the next milestone
* some shader/data cleanup remains possible afterward

---

## Closeout Criteria

Milestone 24 is complete when:

* forward+ tile/light classification is GPU-driven
* current lit world quads still behave correctly
* shared limits remain coordinated across the checked-in shared config header, C++, and shader builds
* dropped-light and dropped-reference diagnostics remain visible
* Vulkan, Metal, and DirectX 12 remain aligned for the GPU forward+ slice

## Current Closeout Note

As of **April 19, 2026**, milestone 24 is implemented and effectively closed for active feature work, but it should remain in `docs/` rather than `docs/archive/` until DirectX 12 native runtime validation is completed.

Current practical closeout state:

* Vulkan runtime validation is current
* Metal runtime validation is current
* DirectX 12 shared-code compilation is current
* DirectX 12 native runtime validation is still pending a Windows follow-up pass
* the current DirectX 12 path uses upload-backed staging for CPU-authored storage-buffer writes before compute, which is considered acceptable for the milestone 24 slice and should be preserved as a documented backend-specific implementation detail rather than treated as a retired CPU fallback returning through the renderer
