# Carrot Game Engine - Milestone 23

**Last Updated:** April 18, 2026
**Title:** Compute-Capable RHI Contract and Renderer Execution Foundations
**Status:** Planned
**Focus:** Add the minimum honest cross-backend contract needed for compute work, storage resources, indirect execution, and renderer-owned GPU execution planning before more renderer growth lands on the wrong architectural seams.

---

## Milestone Goal

Carrot's renderer is in a much better place than it was before the recent cleanup work:

* frame stages are now explicit
* world and non-world responsibilities are more clearly separated
* backend parity is now tied to a practical context-level contract
* forward+ already exists as a real renderer feature

That means the next renderer question is no longer:

* can Carrot render meaningful staged content across Vulkan, Metal, and DirectX 12?

It can.

The next renderer question is:

* whether the RHI and renderer contract are honest enough to support compute-driven work
* whether GPU-written buffers and indirect execution can be added without forcing features through backend-local side doors
* whether later forward+, culling, bloom, shafts, and transition effects will land on a shared renderer foundation or on a pile of special cases

Milestone 23 exists to establish that foundation deliberately.

This milestone is successful if Carrot ends with:

* a practical compute-capable RHI contract
* buffer/resource usage support for GPU-written renderer data
* context-level compute dispatch support across Vulkan, Metal, and DirectX 12
* explicit enough resource-state and pass-boundary rules for later multi-pass work
* indirect draw readiness for future GPU-driven world execution

---

## Scope Summary

Milestone 23 is:

* an RHI contract milestone
* a renderer execution-foundation milestone
* a compute readiness milestone
* a parity-preservation milestone

Milestone 23 is not:

* a GPU-driven renderer rewrite
* a bloom milestone
* a battle-swirl milestone
* a bindless-everything milestone
* a giant material-system milestone

The key rule is:

**Renderer growth should land on an honest compute-capable contract before it lands on compute-heavy features.**

---

## Why This Milestone Comes Next

Carrot's current renderer still does the expensive structural work on the CPU:

* quad sorting and batch construction happen on CPU
* quad expansion into vertex/index geometry happens on CPU
* per-frame uploads are CPU-authored before backend stage recording

That is acceptable for the current slice.
It is not the right substrate for:

* GPU forward+ classification
* GPU culling and batching
* GPU-generated indirect draws
* bloom and later fullscreen post work

If Carrot tries to add those features before the RHI and renderer contract are ready, it risks creating:

* one-backend-first implementation pressure
* renderer exceptions that do not generalize
* fake parity surfaces that exist only on paper
* future cleanup work that is harder than doing the contract correctly now

This is the right time to tighten the foundation before the next wave of renderer growth.

---

## Core Architectural Rule

The shared renderer/RHI contract should describe the real next renderer path, not just the current CPU-quad path plus wishful comments.

That means:

* compute work should be part of the practical context-level contract
* GPU-written buffers should be expressible in shared resource terms
* indirect execution should be introduced through a narrow honest path
* pass boundaries and resource transitions should be explicit enough for renderer growth

If a feature can only be implemented by reaching through backend-local escape hatches, milestone 23 has not gone far enough.

---

## Primary Deliverables

### 1. Practical RHI Contract Expansion

Carrot should identify the smallest real shared contract needed for:

* compute dispatch
* storage-style GPU data
* indirect draw readiness
* renderer-visible resource-state transitions

Required outcomes:

* a documented compute-capable current contract
* less ambiguity around what the renderer may legally ask all backends to do
* no need to smuggle later GPU-driven work through fake or legacy surfaces

### 2. Shared Buffer and Resource Usage Growth

The shared resource model should support the actual data kinds later milestones need.

Required outcomes:

* support for GPU-written/storage-oriented buffer usage
* support for indirect-argument usage
* support for readback where diagnostics or validation genuinely need it
* clearer ownership of CPU-writable versus GPU-generated renderer data

### 3. Compute Pipeline and Dispatch Support

Carrot should gain a practical compute execution path through the live context-level contract.

Required outcomes:

* a shared compute pipeline concept
* context-level dispatch support
* enough shared descriptor/binding structure for storage buffers and compute constants

### 4. Resource State and Pass-Boundary Contract

Later milestones will require GPU work that writes data before later passes consume it.

Required outcomes:

* explicit pass-boundary ownership
* explicit resource-state/synchronization expectations
* fewer backend-local hidden assumptions in renderer behavior

### 5. Indirect Draw Readiness

Carrot does not need a giant command-generation system yet.
It does need a first honest indirect path that later world rendering can target.

Required outcomes:

* one narrow shared indirect draw path
* backend provisioning that matches that path
* no need for a second contract rewrite before GPU-driven world execution

### 6. Documentation and Validation

The contract should remain honest as it expands.

Required outcomes:

* updated docs for the compute-capable slice
* clear parity expectations for Vulkan, Metal, and DirectX 12
* tests or validation scaffolding where practical

---

## Ticket Breakdown

### Ticket 23.1 - Practical RHI Contract Audit for Compute Growth

**Priority:** P0
**Outcome:** Carrot defines the minimum live renderer contract needed for compute-driven work.

#### Why

The current practical contract is still centered on CPU-authored textured-quad stage recording.
That was the right contract for the milestone 22 slice.
It is too narrow for the next renderer phase.

#### Scope

Audit the current context-level contract and define the smallest shared additions needed for:

* compute dispatch
* GPU-written buffers
* resource-state transitions
* indirect draw preparation/consumption

#### Acceptance Criteria

* a documented compute-capable current contract exists
* the contract is framed at the context level rather than as backend-local exceptions
* parity expectations are explicit for Vulkan, Metal, and DirectX 12

### Ticket 23.2 - Shared Buffer and Resource Usage Expansion

**Priority:** P0
**Outcome:** RHI resources can represent the data types later renderer milestones require.

#### Scope

Add shared usage concepts for:

* storage/structured buffers
* indirect argument buffers
* readback where practical and justified

#### Acceptance Criteria

* shared creation APIs can express GPU-written and indirect-consumed renderer data
* native backends provision those usages honestly
* existing textured-quad paths remain intact during the milestone

### Ticket 23.3 - Compute Pipeline and Dispatch Path

**Priority:** P0
**Outcome:** Carrot can execute compute work through the practical shared context path.

#### Scope

Add:

* compute pipeline creation/ownership
* context-level dispatch entry points
* minimal binding support for storage buffers and compute constants

#### Acceptance Criteria

* Vulkan, Metal, and DirectX 12 can all execute a small shared compute workload
* the compute path participates in the live renderer contract
* the implementation remains narrow and understandable

### Ticket 23.4 - Resource State and Synchronization Contract

**Priority:** P1
**Outcome:** GPU-written resources can be consumed safely by later passes without hidden backend assumptions.

#### Scope

Define the renderer/RHI contract for:

* write then later read patterns
* compute to graphics handoff
* graphics to compute handoff where later milestones need it

#### Acceptance Criteria

* pass-boundary/resource-transition expectations are explicit
* the renderer can express later forward+, indirect, and post-effect style data flow cleanly
* backend implementations remain aligned with the shared contract

### Ticket 23.5 - Indirect Draw Plumbing Readiness

**Priority:** P1
**Outcome:** Carrot has a narrow honest indirect execution path available for later world rendering.

#### Scope

Introduce one shared indirect draw form suitable for the future GPU-driven world slice.

#### Acceptance Criteria

* a backend-agnostic indirect draw path exists for the milestone slice
* native backends support it honestly
* later milestones can target it without another contract refactor

### Ticket 23.6 - Regression Coverage and Validation

**Priority:** P1
**Outcome:** The compute-capable contract is protected by docs, tests, and validation notes rather than memory.

#### Acceptance Criteria

* docs are updated to describe the compute-capable current slice
* tests or validation scaffolding cover the new contract where practical
* milestone-closeout notes define what "supported" means for this expanded slice

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. a documented compute-capable practical RHI contract
2. shared resource usage support for storage-style and indirect-style renderer data
3. a real cross-backend compute dispatch path
4. explicit enough synchronization/resource-transition rules for later renderer growth
5. one narrow indirect draw readiness path

If those land cleanly, the milestone succeeds even if:

* later GPU-driven world execution is still a follow-on milestone
* post effects are still later work
* the engine deliberately keeps the first compute-capable contract narrow

---

## Closeout Criteria

Milestone 23 is complete when:

* Carrot has a compute-capable context-level RHI contract
* the renderer can create and use GPU-written/storage-style resources honestly
* compute dispatch exists across Vulkan, Metal, and DirectX 12 for the milestone slice
* resource-state and pass-boundary behavior are explicit enough for forward+, culling, and post work
* indirect draw readiness exists through a narrow shared path
* parity docs remain honest about what is truly supported
