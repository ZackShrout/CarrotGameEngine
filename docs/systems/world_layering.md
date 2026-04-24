# Carrot World Layering

**Last Updated:** April 24, 2026

This document defines the current authored/runtime layering contract between Tiled exports and Carrot.

For the broader Tiled-authored data contract, see [tiled_authored_data.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/tiled_authored_data.md).

This is effectively a small data API.

If a map uses these authored conventions, the engine will interpret them consistently at runtime.

## Goals

Carrot currently supports three major 2D layering needs:

* object-style anchor sorting
* tile-layer front/back behavior
* tile-layer visibility hiding through authored zones

The system is intentionally explicit.

Carrot should not require layer-name hacks or game-specific render code to express common authored patterns such as:

* roofs that hide when the player walks underneath
* fence or wall-front layers that should cover the player only when appropriate
* pure foreground dressing that should always stay above actors

## Runtime Model

Carrot resolves world rendering through:

1. broad render layer
2. render ordering mode
3. optional visibility-zone-controlled hide/show behavior

### Render Layers

Current broad strata:

* `background`
* `world_back`
* `actors`
* `world_front`
* `effects`
* `debug`
* `ui`

### Render Order Modes

Current ordering modes:

* `explicit_order`
* `anchor_bottom_y`

## Default Behavior

If no special authored properties are present:

* tile layers render as normal world layers
* object layers render through the object path
* Tiled objects imported into the world default to anchor sorting

Current intentionally conservative built-in defaults:

* layers with authored type `Water` default to `background`
* layers with authored type `Roof` default to `world_front`
* object layers default to `actors + anchor_bottom_y`

These defaults are only a fallback. Explicit authored properties should be preferred.

For Tiled layer metadata, Carrot reads the exported layer `class` field as the layer's authored type.

## Tiled Authoring API

Important:

* if your map depends on shared external tileset metadata, save the external tileset as `TSJ`, not `TSX`
* Tiled may default to XML external tilesets (`.tsx`)
* Carrot's supported shared external tileset workflow uses JSON tilesets (`.tsj`)
* shared external `TSJ` tilesets are recommended for reusable layering metadata across many maps
* embedded tilesets are still allowed, but repeated embedded copies can drift over time

For the fuller setup guidance, including recommended `tilemaps/worlds`, `tilemaps/maps`, and `tilemaps/tilesets` layout, see [tiled_authored_data.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/tiled_authored_data.md).

### `VisibilityZone` Objects

Visibility hiding is opt-in.

If a map has no objects with `type = VisibilityZone`, the feature simply does nothing.
This is not a warning and not an error.

To author a visibility zone in Tiled:

* create a rectangle object
* set `type = VisibilityZone`
* add string property `visibility_zone_id`

Example:

* object `type = VisibilityZone`
* property `visibility_zone_id = inn_roof`

Multiple `VisibilityZone` objects may share the same id.
This is the intended way to represent irregular or multi-part interiors.

### Layer And Group Zone Binding

Tile layers can bind to a visibility zone id in two ways:

* directly on the tile layer with `carrot_visibility_zone`
* on the tile layer or a parent Tiled group with `visibility_zone_id`

Current precedence:

1. `carrot_visibility_zone` on the child tile layer
2. `visibility_zone_id` on the child tile layer
3. nearest parent group `visibility_zone_id`
4. no zone binding

If the player is inside any active `VisibilityZone` whose id matches the resolved layer binding, that layer hides.

If a layer references a zone id that has no matching `VisibilityZone`, nothing fails; that layer simply never hides.

### Conditional Front

Use `carrot_conditional_front = true` on a tile layer when that layer should sometimes cover the actor and sometimes sit behind the actor.

Current first-pass behavior:

* the layer renders in the actor-sorted stratum
* per-tile draw quads use bottom-Y ordering against actors

Good use cases:

* fence rails
* bridge rails
* wall fronts
* counter fronts

This should be authored on a separate tile layer from any floor/deck/base tiles that should always remain below the actor.

### Always Front

Use `carrot_always_front = true` on a tile layer when it should always render above actors.

Current behavior:

* the layer renders in `world_front`
* it is not conditional on actor position

Good use cases:

* pure foreground dressing
* hanging trim
* decorative foreground overlays

If both `carrot_always_front` and `carrot_conditional_front` are authored on the same layer, `carrot_always_front` wins.

### Optional Explicit Render Overrides

Tile layers may also use these lower-level overrides:

* `carrot_render_layer`
* `carrot_order_mode`
* `carrot_order`
* `carrot_visibility_tag`
* `carrot_visibility_rule`

Supported `carrot_render_layer` values:

* `background`
* `world_back`
* `actors`
* `world_front`
* `effects`
* `debug`
* `ui`

Supported `carrot_order_mode` values:

* `explicit`
* `explicit_order`
* `anchor_bottom_y`
* `bottom_y`

Supported `carrot_visibility_rule` values:

* `always`
* `hide_when_tag_active`
* `hidden_when_tag_active`
* `show_when_tag_active`
* `visible_when_tag_active`

These are more advanced knobs.
Most Tiled content should use the higher-level authoring patterns above instead.

## Recommended Authoring Patterns

### Roofs

Recommended setup:

* put a building’s roof tiles in a dedicated roof group or roof layers
* set the roof tile layers' Tiled `Class` to `Roof`
* bind that group/layers with `visibility_zone_id = inn_roof`
* author one or more `VisibilityZone` rectangle objects with `type = VisibilityZone`
* give each of those objects `visibility_zone_id = inn_roof`

This is the preferred way to handle odd-shaped buildings.

### Same-Map Interiors

Use this pattern when the player can walk inside a building on the same map and some interior-facing layers should only appear while inside.

Recommended setup:

* author the roof layers with Tiled `Class = Roof`
* bind the roof group/layers with `visibility_zone_id = inn_roof`
* author one or more `VisibilityZone` rectangle objects with `visibility_zone_id = inn_roof`
* put interior front-facing art on a separate tile layer
* set that interior layer to `carrot_always_front = true`
* bind that interior layer to the same zone with `visibility_zone_id = inn_roof` or `carrot_visibility_zone = inn_roof`
* set `carrot_visibility_rule = visible_when_tag_active`

This gives the normal intended result:

* outside the building: roof visible, interior-front layer hidden
* inside the building: roof hidden, interior-front layer visible above the player

This is the recommended first-pass way to author:

* interior wall trim
* shop counters
* hanging indoor foreground details
* same-map inn and shop interiors

### Fences And Wall Fronts

Recommended setup:

* base/floor/walkable tiles stay on normal non-conditional layers
* front-facing rails or wall-front tiles go on a separate layer
* that separate layer gets `carrot_conditional_front = true`
* if a front tile should keep the actor behind a taller stacked front, author `carrot_sort_span_down = <tile count>` on that tile in the tileset

`carrot_sort_span_down` is a tileset-tile property.

Examples:

* `0` or unset: sort from this tile's own bottom edge
* `1`: sort from the bottom edge of the tile one row below
* `2`: sort from the bottom edge two rows below

Current first-pass behavior:

* this only affects tile-layer content using `anchor_bottom_y` behavior such as `carrot_conditional_front`
* the span only extends downward through occupied cells in the same column
* if the expected continuation cell is empty, Carrot clamps the sort anchor to the last occupied row instead of extending through empty space
* if the resolved anchor tile's visible art stops above the tile bottom, `carrot_sort_anchor_offset_y = <pixels>` may be authored on that tileset tile to lift the final sort line upward

### Pure Foreground Dressing

Recommended setup:

* put the art on a dedicated tile layer
* set `carrot_always_front = true`

### Tiled Objects

Imported Tiled objects currently default to simple anchor sorting.

That is the intended default for:

* signs
* chests
* tables authored as one object
* other single-image props

If content truly needs split front/back behavior, it is usually better authored as tiles on separate tile layers rather than as one Tiled object.

## Things To Avoid

* Do not rely on object names to activate visibility zones. Only `type = VisibilityZone` counts.
* Do not mix unrelated roof sets that need different zone ids on the same roof layer/group.
* Do not put bridge floors and bridge rails on the same conditional layer if the floor should always stay below actors.
* Do not author a single Tiled object when the art really wants split base/front layering.
* Do not assume `.world` composition changes runtime layering behavior automatically; `.world` support is currently authored-data preparation, not streamed runtime world behavior.

## Current First-Pass Coverage

The current Milestone 05 slice intentionally covers:

* object anchor sorting
* roof/canopy visibility zones
* conditional-front tile layers
* always-front tile layers
* group-level zone binding inherited by child roof layers
* layer-class-driven roof fallback through `Class = Roof`

## Engine Debug Snapshot

Carrot also captures an engine-side layering snapshot each world draw.

This is not rendered to the screen by default.

Current snapshot data is intended for future tooling, logs, or inspection helpers and includes:

* active visibility-zone ids for the current primary visibility anchor
* whether a visibility anchor was found and where it was in world space
* visibility-region count in the loaded world
* per-frame counts for visibility-bound, conditional-front, always-front, visible, and hidden layers
* a per-layer summary of resolved render behavior for imported tilemap layers

This keeps layering debugability inside the engine without forcing client-facing overlays into normal gameplay.

Not yet covered:

* arbitrary polygon visibility zones
* richer editor tooling
* text/debug labels that explain every final draw decision

Those remain follow-up work, but the current authored/runtime contract is now explicit, test-covered, and suitable for real project use.
