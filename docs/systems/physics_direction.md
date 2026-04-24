# Carrot Physics Direction

**BunnySoft**
**Working system direction**
**Last Updated: April 24, 2026**

---

## 1. Purpose

This document captures the current direction for Carrot's collision and physics-related systems.

Its main job is to separate:

* what Carrot should build now
* what Carrot should build later
* what Carrot should explicitly avoid building too early

For Carrot, "physics" is not primarily about creating a giant general-purpose simulation stack.
It is about building the collision, query, movement, and world-constraint systems that the engine's target games actually need.

---

## 2. Core Philosophy

Carrot should be a **gameplay-first engine, not a simulation-first engine**.

That means:

* collision/query infrastructure matters more than flashy simulation early
* common gameplay operations should be explicit engine features
* gameplay systems should not depend on hidden solver side-effects
* gravity should be opt-in by movement mode, not a global assumption

For Carrot's target games, the right first mental model is:

* build a 2D collision and query engine first
* build kinematic movement on top of it
* add lightweight rigid-body features later only where they are genuinely useful

---

## 3. High-Level System Boundaries

Carrot should separate these concepts clearly.

### 3.1 Collision World

Responsible for:

* collision shapes
* overlap tests
* sweep tests
* raycasts and related queries
* collision filtering
* trigger detection
* world geometry queries

### 3.2 Body / Actor Presence

Responsible for representing a thing that exists in the collision world.

Typical data:

* transform
* active collision profile
* collision layer / mask
* current movement state flags
* optional velocity or motion state

### 3.3 Motor

Responsible for movement and collision resolution rules.

This is where "how motion works" lives.
Examples:

* top-down movement
* platformer movement
* fighter movement
* grounded HD2D surface-following later

### 3.4 Controller / Brain

Responsible for producing intent.

Examples:

* player controller
* AI controller
* replay controller
* script / cutscene controller
* network-driven controller later

The clean model is:

* controller produces intent
* motor consumes intent and resolves movement against the collision world

---

## 4. Carrot's "Physics" Priorities

The intended progression is:

### Phase 1

* static collision
* trigger volumes
* collision layers / masks
* tilemap collision
* object-based collision from authored maps
* point, overlap, ray, and basic sweep queries

### Phase 2

* kinematic movement bodies
* character motors
* sweep-and-slide style movement
* explicit movement results and support state

### Phase 3

* simple dynamic rigid bodies where useful
* optional gravity on those bodies
* limited restitution / friction only if actually needed

### Phase 4

* specialized traversal and support models
* grounded surface-following movement for HD2D traversal
* additional shape support and richer queries if demanded by real game content

### Phase 5

* specialized extras only when justified by actual projects

---

## 5. What Carrot Should Build Now

For the near-term engine, the important deliverable is not "full physics."
It is a **collision/query and world-constraint layer**.

That means the early system should prioritize:

* tile / map blocking
* trigger volumes
* actor/world movement blocking
* simple sliding against walls and corners
* world queries for interaction, line-of-sight, placement, and tools
* clean authored collision import from Tiled

This is the work that expands the range of games Carrot can support immediately.

### 5.1 Current Implemented Slice

Carrot now has a working first-pass implementation of this direction:

* `collision_world_t` provides layers / masks, static colliders, point queries, overlap queries, raycasts, and AABB sweeps
* `tile_collision_field_t` exists as an engine-owned static collision representation
* Tiled tileset rectangle collision can populate runtime static world blocking
* baked static colliders now use a uniform-grid broadphase before narrowphase checks
* tile-field point/overlap/raycast/sweep queries clip to relevant cells instead of polling whole fields
* Tiled object-layer trigger rectangles can populate runtime trigger world objects
* world-object collision participation is now explicit: dynamic actor bodies and trigger volumes are distinct roles
* the current player path uses explicit top-down kinematic movement with sweep-and-slide resolution against static collision
* trigger overlap changes are surfaced to gameplay as explicit enter / exit events
* the engine now has toggleable debug visualization for map collision and object colliders, including the player

This is intentionally still a narrow, gameplay-first slice rather than a broad simulation layer.

---

## 6. What Carrot Should Not Build Too Early

Carrot should avoid expanding too early into:

* deep impulse-solver complexity
* joints everywhere
* soft bodies
* cloth
* fluids
* large dynamic stacks of interacting bodies
* polygon-heavy shape support without real need
* a fake "universal physics engine" scope

Those features are not where Carrot gets the most leverage first.

---

## 7. Collision World Model

Carrot's world should be optimized around this reality:

* most collision is static
* some actors are kinematic
* relatively few bodies are truly dynamic

Common examples:

* tilemap walls are static
* map props like fences and rocks are static
* triggers are static or map-authored
* players and NPCs are usually kinematic
* moving doors and platforms are often kinematic
* only a smaller subset of things want actual dynamic simulation later

Current Milestone 28 validated distinction:

* baked map blocking lives in `collision_world_t`
* player/NPC movement bodies are explicit dynamic actor-body participation on world objects
* authored trigger regions are explicit trigger-volume participation on world objects

This matters because Carrot should not architect the whole system around "everything is a free rigid body."

---

## 8. Shape Direction

Carrot should start with a small, disciplined shape set.

Recommended early shapes:

* AABB
* circle where useful for radial checks and simple overlap logic
* capsule as an early-next candidate for smoother character movement
* tile / cell collision as a first-class static world representation

Currently implemented:

* AABB-style collision queries and sweeps
* rectangle-authored Tiled tile collision imported into static colliders
* convex polygon-authored Tiled tile collision imported into static world collision
* ellipse-authored Tiled tile collision imported as convex polygon approximations
* rectangle-authored Tiled trigger regions imported as non-blocking trigger bounds

Delayed shapes:

* concave polygons
* more complex dynamic convex-shape combinations
* richer compound systems beyond what actual games require

Current explicit limitation:

* concave Tiled polygon collision is not supported yet
* collision polylines are not supported yet
* current debug visualization is intentionally simple and focused on inspection rather than a full tooling UI

The engine should leave room for future compound colliders, but should not require them on day one.

---

## 9. Collision Categories

Carrot should treat these as separate concepts:

### 9.1 Locomotion / Blocking Shapes

Used for:

* world blocking
* floor / wall / surface resolution
* movement sweeps

### 9.2 Trigger / Interaction Shapes

Used for:

* pickups
* area transitions
* conversations
* perception / region zones

These should generally not block movement.

### 9.3 Combat Shapes

Used for:

* hurtboxes
* hitboxes
* attack reach checks
* combat query systems

These should not be fused with locomotion shapes by default.

---

## 10. Collision Profiles and Rigs

Carrot should not assume that an actor owns one permanent collider forever.

Instead, the long-term direction should be:

* an actor owns a collision rig
* the rig can expose multiple named collision profiles
* profiles can be switched safely at runtime

Examples:

* `locomotion_horizontal`
* `locomotion_vertical`
* `idle`
* `crouch`
* `jump`
* `knocked_down`

Each profile may contain:

* shape type
* dimensions
* local offsets
* collision filters
* tags or usage intent

### Safe Profile Switching

Profile changes should be a first-class engine operation.

Switching a profile should allow:

* fit testing at the current position
* slight adjustment if allowed by policy
* rejection or deferral if the new profile does not fit
* overlap/contact refresh without solver voodoo

Carrot should avoid designs where gameplay code has to abuse simulation state just to refresh collision.

---

## 11. Movement Architecture

The intended architecture is:

* one collision/query foundation
* multiple movement motors on top of it
* multiple controllers feeding intent into those motors

This avoids mixing:

* player input handling
* AI decisions
* physics queries
* actual movement rules

Carrot should favor:

* one movement core per movement family
* many sources of intent

---

## 12. Motor Families

Carrot should not force every game into one monolithic universal controller.

Instead, different movement families should sit over the same collision/query foundation.

### 12.1 Top-Down Motor

Characteristics:

* planar movement
* no gravity
* blocking and sliding
* trigger interaction
* deterministic-feeling motion

### 12.2 Platformer Motor

Characteristics:

* gravity
* floor / wall / ceiling rules
* jump handling
* air control
* one-way support later if needed

### 12.3 Fighter Motor

Characteristics:

* highly authored movement
* stage boundary clamping
* hard ground-plane rules
* deliberate jump / landing rules
* combat-state-aware movement restrictions

### 12.4 Grounded HD2D Surface Motor

Likely later.

Characteristics:

* support queries
* grounded traversal over topography
* surface projection / clamping
* gravity only when support is lost

---

## 13. Gravity and Support

Carrot should treat gravity as a property of the active movement/body mode, not a property of visual style.

That means:

* top-down actors should generally not use gravity
* platformer actors should
* fighter actors often should, but in a very authored way
* HD2D traversal may or may not use gravity depending on the motor

Important distinction:

* "staying on the ground" is not the same thing as "always being pulled down by gravity"

For grounded traversal, Carrot should eventually support the idea of **support**:

* while support exists, a grounded actor follows valid ground/surface rules
* only when support is lost should falling/gravity take over

This is especially important for future HD2D-style traversal over topography.

---

## 14. Tiled Integration

Tiled should be treated as a world-authoring source, not just a render-data source.

Carrot should eventually import from Tiled:

* visual layers
* tile collision data
* static object collision
* trigger regions
* markers
* actor spawn records
* gameplay metadata

The map should be part of the collision system, not something rendered behind it.

---

## 15. Tile Collision Direction

Tile collision should not begin as "spawn one collider object per solid tile."

Instead, Carrot should use a specialized tile/cell collision representation.

Likely early data:

* solid / non-solid
* basic collision flags
* optional filter/category metadata later

Later tile metadata may include:

* projectile blocking
* vision blocking
* ladder
* one-way
* water
* damaging
* slippery
* slow terrain

The first implementation should stay simpler than the eventual model.

---

## 16. Object Collision Direction

Tiled objects should be able to become:

* static blocking geometry
* triggers
* markers
* runtime actor spawn instructions

Carrot should distinguish between:

### 16.1 Pure Map Collision Objects

Examples:

* fence
* rock
* wall post
* non-interactive decor footprint

These can stay map-owned and static.

### 16.2 Actor-Spawning Objects

Examples:

* breakable barrel
* chest
* NPC
* opening gate
* moving platform

These should become spawn definitions for runtime actor/world systems where appropriate.

---

## 17. Collision Queries

Early query support should aim for:

* point tests
* AABB overlap tests
* raycasts
* sweep tests
* layer / mask filtering

Later support may include:

* shape casts
* richer broadphase queries
* support queries for grounded traversal

---

## 18. Broadphase Direction

Carrot should start simple.

A likely good first broadphase:

* uniform grid

Why:

* easy to reason about
* aligns naturally with tile-based content
* debuggable
* good fit for mostly static 2D worlds

A more complex broadphase such as a dynamic AABB tree should only arrive if the engine truly needs it later.

---

## 19. Events and Movement Results

Carrot should prefer explicit results over hidden solver magic.

Examples of useful explicit state/result data:

* blocked horizontally
* blocked vertically
* grounded
* supported
* entered trigger
* exited trigger
* hit wall / ceiling / floor

Even where event systems come later, the architecture should preserve this explicitness.

---

## 20. Tooling and Debug Expectations

The collision system should be designed with debugability in mind from the beginning.

Important future debug views:

* tile collision visualization
* static object collider visualization
* trigger visualization
* layer / mask visualization
* support-query visualization
* sweep/raycast debug drawing
* imported object/type inspection

This is especially important for authored Tiled content.

---

## 21. Practical Summary

If Carrot's direction had to be reduced to one sentence, it would be:

*build a 2D collision and query engine first, with kinematic gameplay movement as the main feature and lightweight rigid bodies as a secondary later layer.*

That is the cleanest fit for:

* RPGs
* JRPGs
* action RPGs
* platformers
* fighters
* HD2D exploration and traversal

It also aligns with Carrot's larger philosophy:

* common gameplay operations should be native engine features
* not hacks built on top of a simulation-first worldview
