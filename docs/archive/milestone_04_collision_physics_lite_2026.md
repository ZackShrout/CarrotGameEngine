# Carrot Game Engine - Milestone 04

**Last Updated:** April 4, 2026
**Title:** Collision and Physics-Lite Foundation
**Status:** Complete
**Focus:** Introduce Carrot's gameplay-first collision, query, and kinematic world-constraint foundation without expanding immediately into a full rigid-body simulation system.

---

## Milestone Goal

After Milestone 03, Carrot's renderer and frame structure are in much better shape.

The next major engine limitation is that authored worlds are still mostly decorative:

* actors can move through walls
* tilemaps do not yet provide gameplay blocking
* Tiled-authored props do not yet participate in movement constraints
* triggers and map-owned collision are not first-class runtime systems

Milestone 04 is about changing that.

This milestone should make Carrot's worlds meaningfully playable by introducing:

* collision queries
* map blocking
* trigger volumes
* kinematic movement constraints

It should do so in a way that preserves a clean path toward future specialized movement motors and richer physics features.

---

## Scope Summary

Milestone 04 is **not** a "full physics engine" milestone.

It is a **collision/query and gameplay world-constraint milestone**.

The intended near-term direction is:

* collision/query first
* kinematic movement second
* lightweight rigid bodies later

That means this milestone should prioritize:

* tile collision from Tiled-authored map data
* object-layer blocking and trigger data from Tiled
* layers / masks / filtering
* point, overlap, ray, and sweep query support
* basic kinematic movement resolution against static world collision

It should not sprawl into:

* deep rigid-body simulation
* joints / ropes / cloth
* solver-heavy spectacle physics
* broad arbitrary polygon support

For system-level direction, see [physics_direction.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/physics_direction.md).

---

## Current Implementation Status

Milestone 04 now has a working first-pass gameplay collision slice in the engine.

Implemented so far:

* `collision_world_t` with layers / masks, static colliders, point queries, overlap queries, raycasts, and AABB sweeps
* `tile_collision_field_t` as an engine-owned collision source
* Tiled tileset rectangle collision import into runtime static map colliders
* first-pass top-down kinematic player blocking and sweep-and-slide movement against authored world collision
* explicit world-owned collision config for the player collider
* authored trigger rectangle import from Tiled object layers
* trigger overlap tracking with gameplay-facing enter / exit events
* toggleable collision / trigger debug rectangle rendering in the engine debug stage
* focused regression coverage around collision import, movement blocking, unsticking, sliding, and triggers

Current known limitations:

* arbitrary polygon collision from Tiled is not supported yet
* debug visualization is still a first pass and not yet a full editor-grade inspection tool
* the current movement path is intentionally narrow and top-down oriented
* player collider configuration is explicit at runtime, but not yet authored directly from content data

---

## Ticket 1 - Collision World Primitives and Query API

**Priority:** P0
**Outcome:** Carrot has a basic collision world with query primitives and filterable collision categories.

### Why

Before the engine can do meaningful world blocking, triggers, or movement resolution, it needs a usable collision/query foundation.

### Scope

Introduce the basic collision-world model and query surface:

* collision shapes suitable for early Carrot needs
* collision layers / masks
* point queries
* overlap queries
* raycasts
* basic sweep tests

Likely early shape focus:

* AABB
* tile/cell collision representation
* optional circle if it proves useful early

Current tightened Ticket 1 target:

* `collision_world_t`
* collision layers / masks
* static registered colliders
* AABB overlap tests
* point queries
* raycasts
* AABB sweep tests for movement-style queries
* `tile_collision_field_t` as a first-class static collision source

Current recommended boundary:

* keep Ticket 1 focused on engine-owned collision primitives and query APIs
* do not require full Tiled runtime population in the same first slice if that muddies ownership
* it is acceptable for Ticket 1 to define the tile collision field and query path first, with Ticket 2 handling authored import/runtime population

Likely first concepts:

* `collision_world_t`
* `collision_layer_t`
* `collision_mask_t`
* `collision_aabb_t`
* `static_collider_t`
* `tile_collision_field_t`
* `raycast_hit_t`
* `sweep_hit_t`
* shared query result types where appropriate

### Acceptance Criteria

* The engine has a basic collision/query API.
* Queries can filter by collision categories.
* The design does not assume every object is a rigid body.
* The first collision slice is not overcommitted to rigid bodies, trigger events, or full authored runtime integration before the query foundation exists.

### Current Status

Implemented.

---

## Ticket 2 - Tiled Tile Collision and Static Object Collision Import

**Priority:** P0
**Outcome:** Tiled-authored map data can produce real blocking and trigger-ready collision data at runtime.

### Why

For Carrot's target games, the map is a gameplay-authoring source, not just rendered scenery.

### Scope

Expand the Tiled import/runtime path so a loaded map can contribute:

* tile collision data
* static object-based collision
* trigger-volume definitions
* collision-related metadata where needed

This should clearly distinguish:

* map-owned static collision
* trigger regions
* actor spawn definitions for future runtime objects

### Acceptance Criteria

* Tilemaps can provide real blocking data.
* Tiled object layers can provide static blocking objects and trigger definitions.
* The runtime distinction between static map collision and dynamic actors is clear.

### Current Status

Partially implemented.

What exists now:

* Tiled tileset rectangle collision is imported and instantiated as static blocking colliders
* Tiled object-layer trigger rectangles are imported as non-blocking trigger world objects

Still to expand later if needed:

* broader object-layer static blocking authoring patterns
* richer collision metadata import beyond the current narrow slice

---

## Ticket 3 - Kinematic Actor Movement Against Static Collision

**Priority:** P0
**Outcome:** Actors can move through the world using explicit, stable collision resolution instead of passing through authored geometry.

### Why

Collision data only becomes useful gameplay infrastructure once actors can actually use it for movement.

### Scope

Add a first-pass kinematic movement path for the existing gameplay/world layer:

* attempted movement against static world blocking
* blocking and sliding where appropriate
* explicit result/state from movement resolution

Initial target:

* top-down planar movement
* no gravity assumptions

This should improve real game support quickly while staying narrow.

### Acceptance Criteria

* A movement-controlled actor can no longer pass through basic world blocking.
* Top-down movement works without gravity hacks.
* Movement resolution returns useful explicit state rather than depending on hidden solver behavior.

### Current Status

Implemented for the current top-down player path.

---

## Ticket 4 - Trigger Volumes and Interaction-Oriented Collision

**Priority:** P1
**Outcome:** Trigger/interaction spaces become a first-class concept rather than a side-effect of proximity logic alone.

### Why

Carrot will need authored trigger regions for:

* map transitions
* cutscene/camera zones later
* interaction helpers
* region-based gameplay

### Scope

Add first-pass trigger support:

* trigger-only collision shapes
* trigger queries or events
* basic integration with authored Tiled regions

This should stay clearly separated from blocking/locomotion collision.

### Acceptance Criteria

* Trigger volumes are a real engine concept.
* Triggers do not block movement by default.
* Trigger handling fits naturally with the same collision/filter model as blocking collision.

### Current Status

Implemented as a first pass.

Current runtime shape:

* Tiled-authored trigger rectangles import as world objects with trigger data and non-blocking collision bounds
* trigger overlap changes are surfaced to gameplay as enter / exit events
* the sandbox currently logs those gameplay trigger events as the current gameplay response path

---

## Ticket 5 - Documentation, Debug Views, and Verification

**Priority:** P2
**Outcome:** The collision foundation is documented and inspectable enough to build on confidently.

### Why

Collision systems become painful quickly if they cannot be visualized or understood.

### Scope

Document the current collision model and add practical verification support.

Suggested outputs:

* doc updates for the collision/query model
* clear statement of "now" versus "later"
* initial debug visualization hooks where feasible
* focused regression tests around collision import and movement blocking

### Acceptance Criteria

* The collision direction and current scope are documented.
* The map-to-runtime collision path is discoverable later.
* The engine has enough verification support to catch obvious regressions.

### Current Status

Implemented as a first pass.

Completed in this ticket slice:

* focused verification coverage is in place for collision import, movement constraints, and trigger behavior
* the milestone and system-direction docs capture the implemented first pass and current boundaries
* collision debug visualization now supports separate map-collision and object-collider visibility toggles
* object colliders can opt into debug display individually, while map collision remains a category toggle
* the sandbox has a small on-screen legend for current collision debug visibility state

---

## Explicit Non-Goals

Milestone 04 should **not** expand into:

* full rigid-body simulation
* deep impulse solver work
* joints, ropes, cloth, or fluids
* a one-size-fits-all universal character controller
* full grounded-HD2D surface-following traversal
* full combat hitbox/hurtbox systems

Those are important future topics, but not the right scope for the first collision milestone.

---

## Why This Milestone Comes Next

After Milestone 03, Carrot's renderer stopped being such a structural ceiling.

The next strongest engine-level expansion is to make authored spaces actually behave like gameplay spaces.

This milestone unlocks:

* real movement blocking
* map-driven collision authoring
* trigger regions
* clearer foundations for future top-down, platformer, fighter, and HD2D movement systems

It is the highest-leverage next step for making Carrot support a wider range of real games.

---

## Completion Note

Milestone 04 is complete as a gameplay-first collision foundation milestone.

It delivered:

* engine-owned collision/query primitives
* authored runtime blocking from Tiled collision rectangles
* first-pass kinematic player movement constraints
* authored trigger volumes with gameplay-facing events
* practical debug/verification support for inspecting collision state

Future work can and should build on this, but the milestone’s intended first-pass scope has been met.
