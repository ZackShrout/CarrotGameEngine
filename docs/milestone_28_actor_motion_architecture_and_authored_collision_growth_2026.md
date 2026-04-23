# Carrot Game Engine - Milestone 28

**Last Updated:** April 23, 2026
**Title:** Actor Motion Architecture and Authored Collision Growth
**Status:** Planned
**Focus:** Separate controller intent from movement execution, grow Carrot's gameplay-first collision architecture beyond narrow AABB-only assumptions, bake authored Tiled collision into runtime-friendly static world data, and prove the result with authored NPC patrol behavior.

---

## Milestone Goal

Carrot's current movement/collision slice proved an important point:

* authored worlds can now block actors
* top-down player movement can interact with collision meaningfully
* triggers and interaction-facing world data are no longer just decorative

That foundation is real.
It is also now too narrow for the kinds of games Carrot is meant to support comfortably.

The next movement/collision question is:

* whether controller intent and locomotion are truly separate engine concepts
* whether AI, player, and scripted control can all drive movement through the same engine-owned movement seam
* whether authored world collision can move beyond "lots of little rectangles"
* whether static map collision is being represented and queried in a way that scales like an engine should

Milestone 28 exists to move Carrot from "first playable blocking" to "real game-usable actor motion and authored collision architecture."

This milestone is successful if Carrot ends with:

* clear controller / motor / body separation for the validated slice
* authored NPCs that can patrol along Tiled-authored path data
* locomotion and animation driven through the new movement architecture
* support for non-rect authored collision from Tiled in the milestone slice
* baked static world collision data that avoids naive per-tile collider spam
* broadphase-assisted collision/query behavior instead of brute-force world polling

---

## Scope Summary

Milestone 28 is:

* an actor-motion architecture milestone
* a gameplay-first collision growth milestone
* an authored Tiled collision milestone
* a static-world collision baking milestone
* an NPC patrol proof milestone

Milestone 28 is not:

* a full rigid-body physics milestone
* a generic simulation-first physics-engine milestone
* a pathfinding/navigation mesh milestone
* a dynamic arbitrary-polygon collision-everywhere milestone
* a "solve every possible movement style" milestone

The key rule is:

**Milestone 28 should make authored motion and collision scale like an engine-owned gameplay system, not like a collection of sandbox-local special cases.**

---

## Why This Milestone Comes Next

After Milestone 27, Carrot should know what its canonical 2D renderer path is.
The next structural blocker to "real game-capable engine" work is actor/world interaction.

Today, Carrot's movement and collision slice is valuable but still carries several important constraints:

* controller meaning and locomotion behavior are too tightly linked
* the current player path is intentionally narrow and top-down oriented
* collision support is still centered on rectangles/AABBs
* authored world collision can become too granular and too expensive if every colliding tile remains an independent runtime shape
* current runtime structure is not yet the right seam for AI-driven or scripted movement

That means Milestone 28 is the right next step because it can solve several gameplay-facing risks together:

* decouple who wants motion from how motion is resolved
* improve authored collision truth
* improve collision runtime performance structure
* prove that richer authored content can drive real runtime behavior

---

## Core Architectural Rule

Carrot should separate these concepts clearly:

* **controller / brain**
  Produces intent such as move here, face this way, interact now, follow this route
* **movement motor**
  Resolves that intent into locomotion behavior against collision/query results
* **body / actor collision presence**
  Represents runtime collision participation and movement-relevant shape/state
* **static world collision**
  Represents baked authored map blocking and trigger/query geometry

The clean model is:

* player controller, AI controller, or scripted controller produces intent
* a movement motor consumes that intent
* the motor queries a collision world that is optimized for runtime queries
* animation responds to actual locomotion/facing state rather than to a hard-coded control source

If the milestone adds new features but leaves those ownership lines unclear, it has not gone far enough.

---

## Primary Deliverables

### 1. Controller / Motor / Body Separation

Carrot should make movement architecture reusable across player, AI, and scripted control.

Required outcomes:

* explicit movement-intent flow
* engine-owned movement motor seam
* clear actor/body participation in collision/query systems
* removal of assumptions that "input handling" is the same thing as "movement"

### 2. Authored NPC Patrol Proof Slice

Carrot should prove the new motion architecture through a real authored NPC behavior slice.

Required outcomes:

* NPC authored in Tiled
* patrol route/path authored in Tiled
* runtime import of authored patrol semantics
* AI-style controller driving movement intent
* locomotion and animation behaving correctly along that route

### 3. Non-Rect Authored Collision From Tiled

Carrot should move beyond rectangle-only authored static world collision.

Required outcomes:

* non-rect collision import from Tiled for the validated milestone slice
* real movement/query interaction against those shapes
* debug rendering showing imported runtime truth

### 4. Baked Static World Collision

Carrot should stop treating raw authored tile collision as the permanent runtime representation.

Required outcomes:

* static map collision baking/merging at load/import time
* fewer, larger, more runtime-friendly static world collision structures where appropriate
* preservation of authored blocking meaning while improving runtime structure

### 5. Broadphase-Assisted Query Performance

Carrot should avoid naive "test lots of collision against lots of collision" behavior as the engine grows.

Required outcomes:

* broadphase-friendly runtime collision/query structure
* nearby-candidate query flow before narrowphase testing
* clear distinction between static-world and dynamic-actor query behavior

---

## Ticket Breakdown

### Ticket 28.1 - Movement Intent, Motor, and Body Contract

**Priority:** P0
**Outcome:** Carrot defines and implements the reusable architecture line between controllers, movement motors, and collision bodies.

#### Why

Right now Carrot can prove movement, but not yet the right architecture for multiple control sources.

The engine needs a stable answer to:

* how player intent reaches locomotion
* how AI intent reaches locomotion
* how scripted/cutscene motion will eventually do the same

#### Scope

Introduce the validated movement architecture for the milestone slice:

* movement-intent data shape
* movement motor seam
* body/collision participation model for movement
* runtime state useful for animation/facing/locomotion reporting

#### Acceptance Criteria

* player input is no longer the same concept as movement execution
* AI and future scripted paths can target the same movement seam
* movement state is explicit enough that animation can react to locomotion truth rather than input-only assumptions

### Ticket 28.2 - Player Path Migration to the New Motion Architecture

**Priority:** P0
**Outcome:** The existing player movement path uses the new architecture instead of remaining a special-case legacy path.

#### Why

Milestone 28 should not leave the player path behind on old assumptions while new systems are added beside it.

#### Scope

Migrate the current player movement slice:

* input/controller feeds movement intent
* motor resolves locomotion
* body/collision participation is explicit
* current useful movement behavior remains intact where the milestone does not intentionally change it

#### Acceptance Criteria

* the player path runs through the new architecture
* current gameplay remains functional after migration
* the new architecture is proven on live engine content, not only NPC logic

### Ticket 28.3 - AI Controller and Authored Patrol Route Slice

**Priority:** P0
**Outcome:** An authored NPC can patrol along a Tiled-authored route through the new architecture.

#### Why

This is the milestone's most important end-to-end proof that Carrot has crossed from "movement works" into "multiple control sources can drive shared movement systems."

#### Scope

Add the narrow AI patrol proof slice:

* authored NPC data
* authored patrol route/path data in Tiled
* runtime import of that authored data
* AI controller producing intent
* motor-driven movement and appropriate animation/facing updates

First-pass authored contract for the proof slice:

* `NPC`
  * author as a point object
  * require a non-empty `name` property
  * optional `patrol_path` property means "this NPC uses patrol locomotion"
  * omitted `patrol_path` means "this NPC does not patrol"
  * optional `move_speed` defaults to `2.0`
* `PatrolPath`
  * author as a polyline object
  * require a non-empty `name` property
  * polyline points define patrol traversal order
  * optional `loop` property defaults to `true`
  * optional `ping_pong` property defaults to `false`
  * optional `pause_time` property defaults to `0.0`
  * `loop = false` and `ping_pong = false` means the NPC stops at the final point
  * `ping_pong = true` means the NPC reverses at endpoints
  * `pause_time` applies when the NPC reaches a waypoint within route tolerance

First-pass validation rules for the proof slice:

* unresolved `NPC.patrol_path` references are validation issues
* `NPC` without `patrol_path` is valid
* `NPC` should validate as a point object
* `PatrolPath` should validate as a polyline with at least 2 points
* `PatrolPath` should not set both `loop` and `ping_pong` to `true`

#### Acceptance Criteria

* an NPC authored in Tiled patrols correctly at runtime
* the route is authored, not hard-coded in gameplay glue
* locomotion and animation respond appropriately while patrolling

### Ticket 28.4 - Non-Rect Static Collision Import From Tiled

**Priority:** P0
**Outcome:** Carrot supports richer authored static collision for the milestone slice instead of only narrow rectangle assumptions.

#### Why

Real content such as bridge edges, ramps, trims, cliffs, and angled blockers should not require oversized box collision forever.

#### Scope

Support non-rect authored static collision from Tiled for the milestone slice:

* imported polygon/edge/shape data as appropriate to the chosen implementation scope
* runtime representation suitable for static world blocking/query behavior
* collision debug rendering that shows actual imported shape truth

#### Acceptance Criteria

* the validated slice supports non-rect static world collision from Tiled
* movement and queries can respect those shapes
* debug rendering exposes the resulting runtime geometry clearly

#### Scope Discipline

Milestone 28 does not need to become "arbitrary physics for every polygon case."
It is acceptable to keep the implementation disciplined around:

* static world geometry first
* convex/polygonal or edge-chain forms that fit Carrot's gameplay-first needs
* a narrower actor-shape set than the full world-geometry set if needed

### Ticket 28.5 - Static Map Collision Baking and Merge Rules

**Priority:** P0
**Outcome:** Carrot bakes authored map collision into runtime-friendly static collision data instead of preserving raw per-tile collision spam indefinitely.

#### Why

If each colliding tile becomes its own long-lived runtime collider forever, large maps and dense authored content will produce avoidable runtime overhead.

Carrot should instead transform authored collision into a runtime representation that behaves like an engine-owned system.

#### Scope

Add baking/merging for static world collision where appropriate:

* merge adjacent simple collision regions when meaning is preserved
* retain richer authored shapes where rectangle merging is not enough
* produce a baked static world collision representation at load/import time

#### Acceptance Criteria

* raw authored collision is not the permanent runtime query representation
* baked collider counts are materially more runtime-friendly than naive per-tile expansion where applicable
* authored blocking meaning remains correct after baking/merging

### Ticket 28.6 - Broadphase Structure for Static World Queries

**Priority:** P0
**Outcome:** Static world collision queries stop depending on naive full-world collider polling.

#### Why

As Carrot grows toward more NPCs, richer maps, and more movement/query behavior, naive iteration across large static collider sets will become unacceptable.

#### Scope

Add a broadphase-friendly query structure for static world collision:

* region/spatial partitioning appropriate for Carrot's gameplay-first world model
* nearby-candidate discovery before narrowphase checks
* integration with movement sweeps, overlaps, point queries, and other relevant query types

#### Acceptance Criteria

* static world queries use a broadphase-assisted approach
* the engine does not rely on world-sized collider polling for normal movement/query flow
* diagnostics can show candidate counts and narrowphase counts meaningfully

### Ticket 28.7 - Static vs Dynamic Collision Participation Model

**Priority:** P1
**Outcome:** Carrot's collision architecture distinguishes static world data from dynamic actor participation clearly.

#### Why

The engine should not force map collision, triggers, player bodies, and NPC bodies into one undifferentiated model if their runtime behavior and performance needs are different.

#### Scope

Clarify and implement the participation model for:

* baked static map blocking
* triggers
* dynamic actor bodies
* movement-oriented query flows between them

#### Acceptance Criteria

* static and dynamic participation are distinct in the architecture
* the runtime query path reflects that distinction
* later growth toward more actors does not require unpicking the milestone's design

### Ticket 28.8 - Collision, Motion, and Query Diagnostics

**Priority:** P1
**Outcome:** Carrot exposes enough truth to understand authored collision, baked collision, query counts, and motion behavior.

#### Why

Milestone 28 should not produce a more advanced movement/collision stack that is harder to debug than the first-pass rectangle slice.

#### Scope

Add diagnostics appropriate for the milestone:

* authored collider count vs baked runtime collider count
* broadphase candidate counts
* narrowphase test counts
* query timings where practical
* debug rendering for imported/baked shape truth
* useful motion-state inspection for player/NPC movement

#### Acceptance Criteria

* collision growth remains understandable after the milestone
* performance questions can be investigated without guesswork
* authored NPC patrol and non-rect collision are both inspectable in runtime diagnostics

---

## Showcase Validation Slice

Milestone 28 should prove itself through at least one concrete authored slice that includes:

* a Tiled-authored NPC
* a Tiled-authored patrol route/path
* locomotion through the new motion architecture
* animation that reflects locomotion/facing state
* non-rect static world collision such as a sloped bridge-edge or angled blocker
* debug visualization proving imported and/or baked collision truth

This is important because Milestone 28 should be visibly game-facing, not just architecturally cleaner on paper.

---

## Non-Goals

Milestone 28 should explicitly not grow into:

* full rigid-body simulation
* broad dynamic polygon collision for every shape combination
* deep pathfinding/navigation systems
* a giant AI framework beyond the patrol proof slice
* broad combat behavior systems
* cutscene systems
* every possible movement mode the engine may someday support

Those topics may all matter later.
They are not the purpose of this milestone.

---

## Validation Expectations

Milestone 28 should not be considered complete until it validates:

* the player path on the new motion architecture
* the authored NPC patrol proof slice
* non-rect static collision import from Tiled
* baked static world collision generation
* broadphase-assisted query behavior with meaningful diagnostics
* that collision/runtime cost is materially more scalable than naive per-tile collider polling

---

## Success Criteria

Milestone 28 is succeeding when:

* Carrot has a clear controller / motor / body separation
* AI and player control can drive the same movement architecture
* authored NPC patrol works end to end from Tiled data to runtime locomotion/animation
* non-rect authored collision from Tiled works in the live runtime slice
* static map collision is baked into a runtime-friendly representation
* normal movement/query behavior no longer depends on naive world polling

That would make Milestone 28 the point where Carrot's movement/collision architecture starts behaving like a real engine-owned gameplay system rather than an extended proof of concept.
