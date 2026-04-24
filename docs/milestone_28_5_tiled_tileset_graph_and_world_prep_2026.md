# Carrot Game Engine - Milestone 28.5

**Last Updated:** April 24, 2026
**Title:** Tiled Tileset Dependency Graph and World Preparation
**Status:** Planned
**Focus:** Solidify Carrot's Tiled asset-graph architecture so shared external TSX tilesets become a strong supported workflow, authored dependency invalidation becomes honest, and `.world` files become an explicit architectural input for future streamed overworld work without forcing runtime streaming into this milestone.

---

## Milestone Goal

After Milestone 28, Carrot's Tiled-authored world data is already becoming much more valuable:

* tile-layer front/back behavior is more expressive
* tile metadata such as collision and sort policy is becoming meaningfully engine-facing
* world-authored content is no longer just decorative input

That progress raises the next Tiled question:

* whether shared tilesets are a truly supported long-term workflow or only a partially supported convenience
* whether cooked tilemaps are honestly invalidated when referenced authored dependencies change
* whether Carrot has a real authored dependency graph for Tiled data rather than treating every `.tmj` as an isolated blob
* whether `.world` support can land later as a natural extension of that graph instead of as a second disconnected importer

Milestone 28.5 exists to move Carrot from "TMJ import works for current content" to "Carrot understands authored Tiled data as a reusable dependency graph."

This milestone is successful if Carrot ends with:

* strong support for external `.tsx` tilesets referenced by `.tmj`
* consistent import behavior between embedded and external tilesets for the validated feature slice
* dependency-aware cooked invalidation for referenced Tiled-authored files
* clear documentation that encourages shared project tilesets without hard-enforcing one project structure
* a `.world`-aware architectural foundation that can inspect and validate authored world composition without yet committing to runtime streaming behavior

This milestone is not about finishing streamed overworld loading.
It is about making the authored asset graph clean and honest enough that streamed overworld work later has the right substrate.

---

## Scope Summary

Milestone 28.5 is:

* a Tiled asset-graph architecture milestone
* an external TSX support milestone
* a dependency invalidation milestone
* a Tiled authoring-guidance milestone
* a `.world` preparation milestone

Milestone 28.5 is not:

* a full runtime streaming milestone
* a chunk residency or world-partition scheduler milestone
* an asynchronous IO milestone
* a "load arbitrary giant overworlds in gameplay" milestone
* a broad editor/tool authoring milestone

The key rule is:

**Milestone 28.5 should make Carrot's Tiled import architecture honest and reusable before Carrot tries to build runtime world streaming on top of it.**

---

## Why This Milestone Comes Next

The engine is now reaching the point where tile metadata matters more and more:

* collision is authored and imported
* sort/layering metadata is authored and imported
* shared tileset reuse is now a genuine maintenance concern rather than a theoretical one

That means the current limitation around external `.tsx` tilesets is no longer a small edge case.
It is starting to matter structurally.

Today, the most useful team-facing Tiled workflow would be:

* maps authored as manageable `.tmj` files
* shared tilesets stored once under project assets as `.tsx`
* multiple maps reusing those same shared tilesets
* later, `.world` files composing many `.tmj` into larger authored overworlds

That workflow is exactly the one Carrot should encourage.
But the engine should not encourage it until the importer, invalidation system, and validation story actually support it strongly.

This makes Milestone 28.5 the right next step because it solves several linked problems together:

* shared tileset reuse
* dependency-aware cooked correctness
* clearer import architecture for authored Tiled graphs
* a real foundation for future `.world` support

---

## Core Architectural Rule

Carrot should stop thinking about Tiled input as "one map file goes in, one runtime tilemap comes out."

Instead, for the validated milestone slice, Tiled-authored data should be treated as an authored dependency graph:

* `.world` may reference many `.tmj`
* `.tmj` may reference many `.tsx`
* `.tsx` may reference image content and carry engine-facing tile metadata

The engine should:

* resolve those references explicitly
* import the graph deterministically
* invalidate cooked artifacts honestly when dependencies change
* keep runtime boundaries clear so later streaming work can load authored chunks without rethinking the authoring graph

If the milestone adds TSX parsing but still treats dependency edges as optional bookkeeping, it has not gone far enough.

---

## Boundary Decision For `.world`

Milestone 28.5 should **not** end with runtime `.world` loading and streamed chunk residency.

That would quietly combine two different milestones:

* authored dependency-graph hardening
* runtime world streaming / residency policy

Those are related, but they are not the same problem.

For Milestone 28.5, `.world` should stop at:

* parseability
* authored reference resolution
* validation
* inspectable/importable engine-owned representation
* clear runtime-facing architecture seams for future loading/streaming work

What is intentionally deferred to a later milestone:

* runtime `.world` scene/world loading
* chunk activation/deactivation policy
* chunk streaming based on player/camera position
* async IO and memory residency strategy
* streamed collision/world-object lifecycle behavior

The milestone closeout should be able to say:

* Carrot understands `.world` as authored composition data
* Carrot does not yet claim streamed overworld gameplay support

That is the clean seam.

---

## Primary Deliverables

### 1. External TSX Expansion As A First-Class Import Path

Carrot should fully support `.tmj -> .tsx` tileset references for the validated milestone slice.

Required outcomes:

* external TSX resolution during TMJ import
* tileset image path resolution relative to the TSX file location
* consistent import of the current engine-facing tile metadata slice from TSX
* embedded and external tilesets behaving equivalently for the validated features

### 2. Dependency-Aware Cooked Invalidation

Cooked tilemaps should become stale when any of their real authored dependencies change.

Required outcomes:

* referenced TSX files participate in invalidation
* referenced tileset image files participate in invalidation where appropriate
* import logs/tooling can explain dependency-driven invalidation clearly

### 3. Shared Tileset Workflow Guidance

Carrot should encourage shared tileset reuse without hard-enforcing a single project structure.

Required outcomes:

* documentation for the preferred shared tileset workflow
* explicit statement that embedded tilesets remain supported
* explicit tradeoff documentation around duplication and drift
* validation/import warnings for risky but still allowed setups

### 4. Engine-Owned `.world` Authoring Representation

Carrot should understand `.world` files as authored world-composition input even before runtime streaming exists.

Required outcomes:

* `.world` parsing into engine-owned authored data
* validation of referenced TMJ maps and basic world composition truth
* inspectable world-composition data available to tests and future runtime systems

### 5. Streaming-Ready Architecture Seams

The milestone should prepare runtime loading/streaming without actually implementing it.

Required outcomes:

* explicit authored graph boundaries between world, map, tileset, and image dependencies
* no importer/runtime assumptions that every map must be loaded in isolation forever
* clear place for a later runtime chunk loader/residency system to plug in

---

## Ticket Breakdown

### Ticket 28.5.1 - External TSX Resolution and Expansion

**Priority:** P0
**Outcome:** A TMJ that references external TSX tilesets imports into the same effective engine-owned tileset model as an equivalent embedded tileset.

#### Why

This is the milestone's most important enabling step.
Without it, the preferred shared-tileset workflow is not genuinely supported.

#### Scope

Implement the external TSX import path:

* resolve `tileset.source` from TMJ entries
* load and parse the referenced TSX
* expand the TSX into the existing `tilemap_tileset_t` model
* preserve current supported metadata from TSX:
  * image/layout data
  * tile animations
  * collision metadata
  * `carrot_sort_span_down`
  * `carrot_sort_anchor_offset_y`

#### Acceptance Criteria

* equivalent embedded and external tilesets produce equivalent imported runtime data for the validated slice
* TSX-relative image paths resolve correctly
* import failure modes are explicit and diagnosable

### Ticket 28.5.2 - Tiled Dependency Graph and Invalidation Truth

**Priority:** P0
**Outcome:** Cooked tilemap artifacts are invalidated when referenced authored Tiled dependencies change.

#### Why

Shared tilesets are not trustworthy if editing the TSX leaves dependent cooked maps stale.

#### Scope

Extend tilemap import/cooked invalidation reasoning to include:

* TMJ source file
* referenced TSX files
* referenced tileset image sources where required by the validated import path

Also improve diagnostic clarity:

* why a cooked map was invalidated
* which dependency changed

#### Acceptance Criteria

* changing a referenced TSX invalidates dependent cooked tilemaps
* dependency-driven cache reuse and regeneration are explainable through logs/tooling
* invalidation is honest for the validated Tiled graph slice

### Ticket 28.5.3 - Shared Tileset Workflow Documentation and Warnings

**Priority:** P1
**Outcome:** Carrot encourages the preferred shared tileset workflow clearly without forbidding embedded tilesets.

#### Why

The engine should guide teams toward maintainable authoring patterns.
It should not pretend all allowed workflows carry the same maintenance risk.

#### Scope

Document and validate the preferred workflow:

* preferred project structure examples
* why shared TSX reuse is recommended
* why embedded tilesets are allowed but carry duplication/drift risk
* warnings for:
  * unresolved external TSX
  * duplicated embedded tileset snapshots where practical to detect
  * unsupported TSX features
  * risky relative-path assumptions

#### Acceptance Criteria

* docs clearly recommend shared reusable tilesets
* docs clearly state that embedded tilesets are still supported
* validation/import warnings communicate risk without blocking legitimate exceptions

### Ticket 28.5.4 - `.world` Parsing and Authored Representation

**Priority:** P1
**Outcome:** Carrot can parse `.world` files into an engine-owned authored representation and validate their map-reference graph.

#### Why

`.world` should become an authored composition concept before it becomes a runtime streaming feature.

#### Scope

Add first-pass `.world` support for the milestone slice:

* parse world file structure
* resolve referenced TMJ files
* preserve authored world composition data such as map paths and spatial placement
* expose the result to tests and future engine code

This ticket does **not** load worlds into gameplay/runtime streaming systems.

#### Acceptance Criteria

* `.world` files can be parsed and validated
* Carrot has an engine-owned authored representation of world composition
* tests can inspect that representation meaningfully

### Ticket 28.5.5 - Runtime Preparation Seams For Future Streaming

**Priority:** P1
**Outcome:** Future streamed overworld loading has clear engine seams to build on without implementing streaming yet.

#### Why

The point of `.world` support is eventually large overworld composition and chunked runtime loading.
But that should arrive through clear runtime seams, not through importer leakage.

#### Scope

Define the future runtime handoff boundaries:

* authored world composition vs runtime loaded-world state
* authored map identity vs loaded map chunk identity
* where collision/world-object import would attach later
* where residency/activation policy would attach later

#### Acceptance Criteria

* the codebase has clear ownership lines for future world-streaming work
* `.world` parsing does not prematurely entangle itself with runtime chunk streaming
* later streaming work can build on existing seams rather than rewriting import boundaries

---

## Implementation Order

Recommended order:

1. Ticket 28.5.1 - external TSX expansion
2. Ticket 28.5.2 - dependency invalidation truth
3. Ticket 28.5.3 - docs and warnings
4. Ticket 28.5.4 - `.world` parsing and authored representation
5. Ticket 28.5.5 - runtime preparation seams

This order keeps the milestone honest:

* first make the preferred shared-tileset workflow actually work
* then make it trustworthy under cooked invalidation
* then document/recommend it
* only then add `.world` preparation on top of the stronger authored graph

---

## Validation Plan

Milestone 28.5 validation should include at least these test classes:

### 1. Embedded vs External Equivalence

Validate that:

* one TMJ with embedded tileset data
* one TMJ referencing an equivalent external TSX

produce equivalent imported tileset/runtime truth for the validated features.

### 2. Shared TSX Reuse Across Multiple TMJs

Validate that:

* multiple TMJs can reference one TSX
* shared metadata such as collision and sort policy survives import consistently

### 3. Dependency Invalidation

Validate that:

* changing the TMJ invalidates the cooked map
* changing the referenced TSX invalidates the cooked map
* changing relevant tileset image dependencies invalidates as intended for the validated slice

### 4. `.world` Composition Validation

Validate that:

* a `.world` file can reference multiple TMJs
* referenced map identities and placements parse correctly
* invalid references are diagnosed clearly

### 5. Non-Goals Stay Non-Goals

Validate that:

* no milestone acceptance language accidentally implies runtime world streaming is complete
* `.world` support remains authored-data/inspection/preparation support only

---

## Recommended Guidance Language

Carrot should eventually document the workflow roughly like this:

* `Preferred`: shared reusable tilesets stored once under project assets and referenced by multiple maps
* `Allowed`: embedded tilesets inside individual maps
* `Tradeoff`: embedded tilesets are easier to start with, but shared tilesets reduce duplication and metadata drift for collision, layering, and future tile behaviors

The point is not to enforce a rigid directory tree.
The point is to make the maintenance consequences clear.

Recommended example structure:

```text
assets/
  tilemaps/
    maps/
      town.tmj
      inn.tmj
      overworld_chunk_00.tmj
    tilesets/
      terrain.tsx
      bridges_and_fences.tsx
      cliffs.tsx
    tilesets/images/
      terrain.png
      bridges_and_fences.png
      cliffs.png
```

This should remain guidance, not a hard requirement.

---

## Explicit Non-Goals

Milestone 28.5 intentionally does not include:

* runtime streamed `.world` gameplay loading
* camera/player-driven chunk activation
* asynchronous chunk IO scheduling
* chunk eviction/residency policy
* runtime stitched overworld navigation across many loaded maps

Those are future milestone concerns once the authored dependency graph is solid.

---

## Closeout Standard

Milestone 28.5 should close only if Carrot can honestly say:

* external TSX tilesets are a strong supported workflow for the validated Tiled slice
* shared tileset reuse is now something the engine can recommend confidently
* cooked tilemap invalidation is honest for the validated Tiled dependency graph
* `.world` files are understood as authored composition data
* runtime world streaming is still deferred and not falsely implied to be complete

That is the right closeout seam because it gives Carrot:

* stronger real-world Tiled workflow support immediately
* less metadata drift risk for game teams
* the right authored substrate for a later streaming-focused world milestone

