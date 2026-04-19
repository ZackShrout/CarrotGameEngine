# Carrot Game Engine - Milestone 25

**Last Updated:** April 18, 2026
**Title:** World Render Item Stream and GPU Quad Culling
**Status:** Planned
**Focus:** Replace CPU-baked world quad geometry with a renderer-owned world render-item stream and introduce GPU-driven world visibility, compaction, and draw preparation while keeping non-world stages intentionally simpler.

---

## Milestone Goal

Carrot's world renderer is now structurally strong enough that its next bottleneck is obvious:

* world content becomes immediate quad submissions
* those submissions are sorted on CPU
* those quads are expanded into full vertex/index streams on CPU
* those CPU-authored buffers are uploaded every frame before recording

That path was the right first real renderer path.
It is not the right long-term world execution model for a renderer that wants:

* larger worlds
* better batching
* GPU visibility work
* later composite/post growth that is not fighting for the same CPU budget

Milestone 25 exists to make the world stage submit render items instead of baked draws.

This milestone is successful if Carrot ends with:

* a renderer-owned world render-item or instance stream
* GPU world visibility filtering and compaction for the milestone slice
* world draw execution that no longer depends primarily on CPU-expanded quad geometry
* tilemap rendering that moves toward chunked/coarse-visibility preparation instead of brute-force forever
* preserved structural separation between world and non-world stages

---

## Scope Summary

Milestone 25 is:

* a world-stage GPU-driven milestone
* a renderer execution-model milestone
* a batching and culling milestone
* a tilemap-preparation milestone

Milestone 25 is not:

* a full-engine GPU rewrite
* a UI/debug/text GPU migration milestone
* an occlusion-culling research milestone
* a giant material-system overhaul
* a bindless-first renderer redesign

The key rule is:

**The world stage should submit render items, then choose CPU or GPU execution internally, rather than forcing gameplay-facing code to know how world geometry is built.**

---

## Why This Milestone Comes Next

After milestone 24, the forward+ lighting path will no longer be the obvious CPU bottleneck.
The next bottleneck becomes the world submission path itself.

The engine currently pays CPU cost for:

* sorting world quads
* expanding them into geometry
* preparing full per-frame vertex/index uploads

That means Carrot can move more renderer work onto the GPU only if it first changes the renderer's internal world-submission shape.

This is also the milestone where tilemaps need to stop depending indefinitely on "walk all drawable cells every frame and emit quads."
That approach was fine for the first slice.
It should not become the permanent world-render architecture.

---

## Core Architectural Rule

Gameplay-facing and world-authoring code should not submit baked geometry.

Instead:

* gameplay/world code should provide render intent
* the renderer should extract that into a world render-item stream
* the renderer should decide whether those items execute through CPU or GPU paths

This milestone should prove that model for the world stage first.

If GPU-driven world work can only be added by pushing world-specific execution rules upward into gameplay-facing submission code, milestone 25 has not gone far enough.

---

## Primary Deliverables

### 1. World Render Item Extraction Layer

Carrot should introduce a renderer-owned world render-item stream.

Required outcomes:

* sprites, tile objects, and tilemap content can all map into a world render-item representation
* extraction is separate from final draw execution
* the renderer is no longer structurally bound to immediate quad baking

### 2. Tilemap Chunking and Coarse Visibility Direction

Tilemaps should move toward a world-friendly render-data structure.

Required outcomes:

* chunked tilemap render preparation
* coarse visibility behavior where it is the right fit
* preserved authored layer ordering and visibility-zone semantics

### 3. GPU World Visibility Filtering and Compaction

The world stage should gain GPU-driven visibility work.

Required outcomes:

* viewport/frustum-visible filtering for world items
* visible-item compaction into a draw-ready list
* preserved order/bucket correctness for the validated world slice

### 4. GPU-Ready World Draw Execution

The world stage should execute from GPU-ready instance or indirect data.

Required outcomes:

* no primary dependence on CPU-expanded per-frame quad geometry
* a narrow world draw path that uses the new GPU-driven preparation
* preserved backend parity for the milestone slice

### 5. Non-World Stage Scope Discipline

Carrot should not overexpand this milestone into unrelated renderer complexity.

Required outcomes:

* UI/debug/text/log remain on simpler honest paths unless explicitly promoted later
* world-stage complexity stays world-stage-local
* renderer architecture becomes clearer rather than broader

---

## Ticket Breakdown

### Ticket 25.1 - World Render Item Extraction Layer

**Priority:** P0
**Outcome:** The world stage extracts a stable renderer-owned item stream before execution decisions are made.

#### Why

Right now the renderer is still too closely tied to immediate quad baking.
That is the architectural seam that needs to move before GPU-driven world work can be clean.

#### Scope

Define a world render item structure that can carry:

* rect/transform data
* UVs and color
* texture/material identity
* sort/bucket key
* bounds/visibility metadata
* stage-local world rendering metadata

#### Acceptance Criteria

* the world stage produces a render-item stream before final execution
* sprites, tile objects, and tilemap-backed content can all map into it
* renderer code is structurally less tied to immediate baked quads

### Ticket 25.2 - Tilemap Chunking and Coarse Visibility Preparation

**Priority:** P0
**Outcome:** Tilemap world rendering moves toward chunked render preparation instead of perpetual brute-force cell traversal.

#### Scope

Introduce chunked tilemap render data suitable for the world stage while preserving:

* current layer ordering behavior
* visibility-zone semantics
* object-layer rendering rules where they remain part of the world slice

#### Acceptance Criteria

* tilemap rendering has a chunk/coarse-visibility direction in the live renderer path
* authored visibility and layering semantics remain correct
* the renderer is no longer architecturally dependent on walking every drawable tile every frame forever

### Ticket 25.3 - GPU World Item Culling and Compaction

**Priority:** P0
**Outcome:** The GPU filters and compacts visible world items for the milestone slice.

#### Scope

Add GPU work that:

* rejects non-visible world items
* compacts visible items into a draw-ready list
* preserves the validated ordering/bucketing rules required by the current world slice

#### Acceptance Criteria

* world visibility filtering is GPU-driven for the milestone slice
* the output is usable by later draw execution without CPU re-expansion
* correctness is preserved for the tested world slice

### Ticket 25.4 - GPU World Draw Execution Path

**Priority:** P0
**Outcome:** Visible world content draws from GPU-ready instance/indirect data instead of CPU-expanded quad geometry.

#### Scope

Add one narrow world draw path that:

* consumes the compacted visible-item results
* handles the needed texture/material bucketing
* remains compatible with the current world-stage rendering slice

#### Acceptance Criteria

* world textured-quad rendering no longer depends primarily on CPU-generated per-frame full vertex/index data
* draw execution uses instance or indirect-driven GPU-ready data
* backend parity remains part of milestone validation

### Ticket 25.5 - CPU World Path Pressure Reduction and Cleanup

**Priority:** P1
**Outcome:** Obsolete CPU world-geometry pressure is retired or narrowed to limited fallback/debug roles.

#### Scope

Reduce or remove the CPU-side world path that:

* sorts immediate quads
* expands them into baked geometry
* uploads them every frame as the main execution model

#### Acceptance Criteria

* world-stage CPU geometry-baking pressure is materially reduced
* extraction versus execution separation is visible in the code architecture
* comments/docs/tests are updated to the new truth

### Ticket 25.6 - Validation and Scope Discipline

**Priority:** P1
**Outcome:** The milestone closes with a narrow proven world-stage GPU-driven slice instead of accidental renderer sprawl.

#### Acceptance Criteria

* UI/debug/text remain on simpler honest paths unless explicitly promoted later
* native backends are validated for the world-stage milestone slice
* docs describe this as a world-stage GPU-driven milestone, not a total-engine GPU rewrite

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. a renderer-owned world render-item stream
2. tilemap chunking/coarse-visibility preparation in the world path
3. GPU visibility filtering and compaction for the milestone slice
4. GPU-ready world draw execution that no longer primarily depends on CPU-expanded quad geometry
5. preserved stage-boundary clarity between world and non-world work

If those land cleanly, the milestone succeeds even if:

* UI/debug/text stay on the current simpler path
* more advanced occlusion ideas are still later work
* broader material-system growth is deferred

---

## Closeout Criteria

Milestone 25 is complete when:

* the world stage submits render items instead of only baked draws
* world visibility filtering and compaction are GPU-driven for the milestone slice
* world draw execution no longer depends primarily on CPU-expanded per-frame geometry
* tilemap rendering has a chunk/coarse-visibility direction compatible with the new path
* non-world stages remain structurally simpler and honest
