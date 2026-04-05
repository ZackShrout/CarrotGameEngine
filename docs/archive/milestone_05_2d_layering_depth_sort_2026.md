# Carrot Game Engine - Milestone 05

**Last Updated:** April 5, 2026
**Title:** 2D Layering and Depth-Sort Behavior
**Status:** Complete
**Focus:** Deliver a real engine-owned solution for 2D world layering, occlusion, and depth-sort behavior so authored spaces render correctly without pushing sort hacks into gameplay code.

---

## Milestone Goal

Milestone 04 made Carrot's authored worlds playable.

Actors now collide with authored spaces, map collision is real, triggers are real, and the engine has a much stronger gameplay foundation than it did before.

The next structural ceiling is visual layering.

Carrot can now render a playable world, but it still does not have a complete engine-level answer for common 2D world visibility problems such as:

* fences that should partially occlude actors
* bridges and overhangs that want split foreground/background behavior
* roofs or canopy-style cover that should draw differently based on actor position
* authored props whose visual layer behavior is more complex than a single bottom-Y sort key
* mixed tilemap/object/actor scenes where "just sort by one number" is not enough

Milestone 05 is about solving that as an engine system.

This should not be framed as a demo feature, a sandbox-specific trick, or a one-off authored workaround.

It should produce a real renderer/world-layer model that future Carrot games can rely on.

---

## Scope Summary

Milestone 05 is **not** just "make sorting a little better."

It is a **2D world layering and authored occlusion milestone**.

The intended near-term direction is:

* engine-owned layer semantics first
* authored/runtime visibility rules second
* richer special cases only where they fit the model cleanly

That means this milestone should prioritize:

* clear layer responsibilities across tilemaps, tile objects, actors, and future occluders
* stronger renderer-owned sort behavior for common top-down / hybrid 2D scenes
* authored support for partial occluders and split foreground/background presentation
* discoverable runtime behavior and debugability
* verification scenes that pressure-test the known failure cases

It should not sprawl into:

* a full lighting/shadow milestone
* arbitrary per-project render hacks
* a giant cutscene visibility framework
* solving every possible HD2D traversal case in one pass
* a purely sandbox-local fix that bypasses engine boundaries

---

## Why This Milestone Comes Next

Milestone 03 established a stronger render pipeline.

Milestone 04 established playable authored worlds.

The next major problem is that Carrot still cannot always present those authored worlds correctly.

Right now, the engine has a useful stepping stone:

* `anchor_bottom_y` ordering works for a narrow set of actor/tile-object cases

But it is still only a stepping stone.

Known problem classes remain:

* fence and railing cases
* bridge / underpass style layering
* roof / canopy / awning occlusion
* partial occluders that should not behave like full foreground layers
* authored objects that need explicit visibility semantics beyond one sort anchor

Milestone 05 should turn those from "known future problems" into an engine-level solution space.

---

## Ticket 1 - Layering Model and Authored Semantics

**Priority:** P0
**Outcome:** Carrot has a clear engine-level model for 2D world layering responsibilities and authored visibility semantics.

### Why

Before improving sort behavior, the engine needs a stable model for what kinds of layered world behavior it is trying to represent.

### Scope

Define and document the layering model across:

* tilemap layers
* tile objects
* actors
* future partial occluders / visibility helpers

This ticket should answer questions such as:

* what should be handled by static render layer alone
* what should be handled by sort-within-layer behavior
* what needs explicit authored foreground/background split behavior
* what runtime concepts should exist instead of being inferred ad hoc

Likely outputs:

* updated docs for the current and intended 2D layering model
* a tighter vocabulary for layers, occluders, sort anchors, and split-visibility authored features
* a concrete list of supported first-pass authored behaviors

### Acceptance Criteria

* The intended 2D layering model is documented clearly enough to implement against.
* Known fence/bridge/roof cases are described explicitly rather than hand-waved.
* The model is engine-owned and not expressed as game-specific workaround logic.

### Current Progress

Implemented first pass:

* engine-owned layer semantics now resolve per tilemap layer instead of flattening all tile layers into one runtime bucket
* the current authored/runtime contract is documented in [world_layering.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/world_layering.md)
* tile layers and object layers can now override defaults with explicit Carrot-authored render properties
* Tiled group-level `visibility_zone_id` can now flow to child roof layers

---

## Ticket 2 - Runtime Layering and Depth-Sort Foundation

**Priority:** P0
**Outcome:** The renderer/world path supports a broader real layering model than a single `anchor_bottom_y` sort rule.

### Why

Carrot needs a stronger runtime rendering foundation before authored occlusion features can behave predictably.

### Scope

Extend the world/render path so it can represent:

* explicit world layer groups where needed
* sortable actor/object behavior within those groups
* authored foreground/background separation where appropriate
* renderer-owned ordering rules that stay understandable

This ticket should preserve the good parts of the current path:

* explicit layer intent
* renderer-owned ordering policy
* authored scene rendering through engine paths

But it should move beyond the current narrow sort mode.

### Acceptance Criteria

* Carrot has a stronger runtime layering model than a single anchor-based sort mode.
* The design remains renderer-owned rather than scattering order decisions through gameplay code.
* The runtime model leaves room for authored partial-occluder behavior in later tickets of this milestone.

### Current Progress

Implemented first pass:

* the renderer now resolves tilemap layers through per-layer semantics
* built-in defaults are currently conservative and intentionally centered on `water` and `roof` patterns
* object-layer props continue to sort through renderer-owned `anchor_bottom_y` behavior
* explicit `carrot_conditional_front` and `carrot_always_front` layer behaviors now exist for tile content

---

## Ticket 3 - Partial Occluders and Split Visibility Cases

**Priority:** P0
**Outcome:** Common authored occlusion cases such as fences, bridges, and roof-like foreground elements have a real engine-supported path.

### Why

This is the actual user-facing reason the milestone exists.

If Carrot cannot solve these cases, it still lacks a trustworthy 2D world presentation model.

### Scope

Add first-pass support for authored/runtime behaviors such as:

* fence-style partial foreground occlusion
* bridge / overpass style split draw behavior
* roof / canopy / awning style foreground coverage
* authored elements that need a foreground-facing pass separate from their base world placement

The implementation does not need to solve every possible exotic case immediately, but it should solve the common cases intentionally and cleanly.

### Acceptance Criteria

* Carrot can render at least the common fence, bridge, and roof-style cases correctly through engine-owned behavior.
* The solution is based on clear authored/runtime concepts rather than one-off hardcoded exceptions.
* The resulting model is suitable for future real projects, not just the current sandbox map.

### Current Progress

Implemented first pass:

* roof/canopy-style hide behavior now has an engine path through authored `VisibilityZone` rectangles
* the sandbox town content now includes explicit per-building visibility zones and matching roof-layer zone ids to exercise that path
* conditional tile-front layers now provide a first usable fence/wall/rail layering path

---

## Ticket 4 - Debug Views and Verification Support

**Priority:** P1
**Outcome:** Layering behavior is inspectable and regression-resistant enough to build on confidently.

### Why

Layering systems get confusing quickly when the engine cannot explain why something rendered in front of something else.

### Scope

Add practical verification support for the layering model:

* focused verification coverage for known problem cases
* debug views or engine-owned inspection data where useful
* clear documentation of the authored-to-runtime layering path
* regression coverage for ordering/visibility behavior where feasible

Likely useful outputs:

* town-map validation of fence/bridge/roof style cases
* engine-owned layering snapshot data for future tooling or logging
* docs that show what authored data is expected for the supported patterns

### Acceptance Criteria

* The layering path is inspectable enough to debug intentionally.
* Known failure cases have explicit verification coverage.
* Future regressions are easier to catch than they are today.

### Current Progress

Implemented first pass:

* visibility-region debug overlays now exist in the world debug model
* regression tests now cover visibility zones, group inheritance, conditional-front layers, and always-front layers
* engine-side layering debug snapshots now capture active visibility-zone state and per-layer resolved behavior without adding client-facing overlays

---

## Milestone Outcome

Milestone 05 is complete.

Carrot now has a real engine-owned authored/runtime layering model built around:

* object anchor sorting by default
* explicit tile-layer conditional front behavior
* explicit tile-layer always-front behavior
* authored `VisibilityZone` roof/canopy hiding
* Tiled group inheritance for zone binding
* engine-side inspection data and regression coverage

What this milestone intentionally does not claim:

* every possible exotic occlusion case is solved forever
* single-image Tiled objects can express split front/back rendering without authoring changes
* future advanced overpass or tunnel cases will never want additional helpers

But it does remove the previous structural ceiling.

Carrot now has an understandable, documented, and Tiled-authored path for the common 2D layering cases the sandbox town needs, and future projects can build on the same model instead of reintroducing renderer heuristics or gameplay hacks.

---

## Explicit Non-Goals

Milestone 05 should **not** expand into:

* a full lighting and shadows milestone
* a universal cinematic visibility system
* a giant editor tooling milestone
* every possible HD2D traversal/rendering edge case
* replacing gameplay/world architecture with render-specific hacks

Those are important future topics, but not the right scope for the first real layering milestone.

---

## Success Definition

Milestone 05 is successful if:

* Carrot has a documented engine-owned 2D layering model
* common authored occlusion cases render correctly
* the renderer/world path is stronger than simple bottom-Y sorting
* the solution feels like durable engine infrastructure rather than a sandbox patch
* future games can rely on the system without having to reinvent layering rules themselves
