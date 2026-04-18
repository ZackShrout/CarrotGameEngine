# Carrot Tiled Authored Data Contract

**Last Updated:** April 17, 2026

This document defines the current authored-data contract between Tiled exports and Carrot.

It is meant to answer two questions clearly:

* what Carrot intentionally supports from Tiled today
* what authored conventions Carrot expects when a map uses engine-facing features

This is a living contract, not a promise that every Tiled feature is already supported.

---

## Current Supported Foundation

Carrot currently supports these Tiled-backed foundations:

* orthogonal tilemaps
* tile layers
* object layers
* group layers flattened into runtime layers with inherited properties
* tileset texture/image metadata
* tileset-defined animated tiles
* tileset rectangle collision import
* object-layer placed tile objects
* object-layer markers and typed gameplay objects
* point objects as first-class authored marker geometry
* imported polygon and polyline geometry metadata on Tiled objects
* rectangle trigger regions
* rectangle visibility regions through `type = VisibilityZone`
* explicit world-layering metadata such as:
  * `visibility_zone_id`
  * `carrot_visibility_zone`
  * `carrot_conditional_front`
  * `carrot_always_front`

Current intentionally unsupported or only partially supported examples include:

* infinite maps
* non-orthogonal map orientations
* polygon collision import
* ellipse collision import
* polyline collision import
* text objects as runtime-authored features

Unsupported features remain non-fatal when practical, but they should still be diagnosed intentionally.

---

## Core Authoring Rules

### Object `type` Matters

Carrot uses Tiled object `type` as part of its authored contract.

Example:

* `type = VisibilityZone` activates visibility-zone behavior

Object name alone is not a contract surface unless a specific runtime system says otherwise.

### Layer Authored Type Matters Too

For Tiled layers, the structural JSON `type` field is already used by Tiled for values like `tilelayer`, `objectgroup`, and `group`.

So the engine-facing authored layer type should come from the Tiled layer `Class` field, which exports as `class` in TMJ.

Example:

* layer class `Water` can be used for Water-specific engine defaults
* layer class `Roof` can be used for roof/front-layer defaults

This keeps authored layer meaning explicit without overloading layer names.

### Properties Are Explicit API

Carrot-authored custom properties should be treated like data API, not informal notes.

Examples already in use:

* `visibility_zone_id`
* `carrot_visibility_zone`
* `carrot_conditional_front`
* `carrot_always_front`

If a property is part of the engine contract, it should be documented and interpreted consistently.

### Current Typed Object Conventions

Carrot currently treats these Tiled object types as first-class authored conventions:

* `Sign`
  * required: `message_id`
* `Container`
  * required: `loot_table`
* `Door`
  * required: `target_marker`
  * required: one of `target_scene` or legacy `target_map`
  * if both `target_scene` and `target_map` are present, `target_scene` is preferred and the map should be cleaned up
* `Trigger`
  * required: `trigger_id`
  * required: `trigger_kind`
* `VisibilityZone`
  * required: `visibility_zone_id`
* `Light`
  * currently supported `kind` values:
    * `ambient`
    * `point`
  * reserved for future expansion:
    * `spot`
  * optional: `behavior = stationary | follow`
  * optional: `color = #RRGGBB | #RRGGBBAA`
  * optional: `intensity = float`
  * point lights require: `radius`
  * follow lights require: `follow_target`

These conventions are now shared across validation and runtime helper code rather than being reinterpreted independently in multiple places.

This built-in set is intentionally small.

Carrot is trying to support the object types that are both:

* common across multiple likely Carrot games
* paired with stable engine-facing meaning

That means the engine should not try to predefine every possible gameplay object type up front.

### Likely Future Engine-Owned Typed Objects

The next most likely object types to become first-class engine conventions are:

* `SpawnPoint`
* `NPC`
* `Pickup`
* `Switch`
* `PatrolPath`

These are documented here as likely future candidates, not as supported built-ins yet.

They should only move into the engine-owned typed set when a real content or runtime need makes their contract clear enough to justify it.

### Unknown Object Types Are Allowed

Carrot should not treat an unknown Tiled object `type` as an import failure by default.

Unknown typed objects should still:

* import as normal Tiled objects
* preserve their authored geometry and custom properties
* remain available to game-side code and future runtime systems

In other words:

* built-in typed objects get shared engine helpers, validation rules, and documented semantics
* unknown typed objects remain valid authored data unless a specific runtime feature says otherwise

This gives game projects room to define their own object types without forking the importer or waiting for engine changes.

### Extension Direction

The intended long-term model is:

* Carrot owns a small engine-level typed object set for common world semantics
* game-side code can layer additional typed conventions on top of imported raw objects
* validation should eventually be extensible enough for game projects to register their own typed object rules when needed

The goal is to keep users from needing custom types too often, while still leaving a clean path when project-specific authored semantics are truly needed.

### `Light` Typed Object Contract

Carrot now treats `type = Light` as an engine-owned authored-lighting convention.

This is the path used to populate scene/world lighting from Tiled-authored data rather than relying on sandbox-local runtime glue.

#### Supported Light Kinds Today

Current supported `kind` values:

* `ambient`
* `point`

Reserved for future expansion:

* `spot`

If a map authors `kind = spot` today, Carrot diagnoses it as not yet supported rather than pretending it behaves like a point light.

#### Supported Behavior Values

Current `behavior` values:

* `stationary`
* `follow`

If `behavior` is omitted, Carrot assumes `stationary`.

`behavior` is ignored for `kind = ambient`.

#### Common Properties

Recognized `Light` properties:

* `kind`
* `behavior`
* `color`
* `intensity`
* `radius`
* `follow_target`

Defaults:

* if `color` is omitted, the light defaults to white
* if `intensity` is omitted, the light defaults to `1.0`
* if no authored ambient light exists, scene ambient defaults to `1.0, 1.0, 1.0, 1.0`

#### Color Format

Carrot accepts both:

* `#RRGGBB`
* `#RRGGBBAA`

Alpha is accepted for authoring convenience but ignored by the engine.

Actual light strength comes from `intensity`, not color alpha.

This is intentional so authors can safely paste colors from tools that export 8-digit hex values without accidentally changing light brightness semantics.

#### Ambient Lights

Author an ambient light like this:

* set `type = Light`
* set `kind = ambient`
* optionally set `color`
* optionally set `intensity`

Ambient lights define scene-global ambient lighting.

Ambient lights ignore:

* object position
* object size
* `behavior`
* `radius`
* `follow_target`

It is acceptable for an ambient light to be authored as a Tiled point object because Carrot ignores spatial meaning for ambient lights entirely.

Current runtime rule:

* at most one ambient light should be authored per scene-backed map
* if multiple ambient lights are authored, Carrot warns and uses the first one

#### Point Lights

Author a stationary point light like this:

* set `type = Light`
* set `kind = point`
* optionally set `behavior = stationary`
* set `radius`
* optionally set `color`
* optionally set `intensity`

Point lights use the authored object position as their world-space origin.

Current runtime behavior intentionally does not infer point-light radius from Tiled object width or height.

Use the explicit `radius` property instead.

#### Follow Lights

Follow lights are currently authored as point lights with a runtime behavior:

* set `type = Light`
* set `kind = point`
* set `behavior = follow`
* set `follow_target`
* set `radius`
* optionally set `color`
* optionally set `intensity`

Current supported `follow_target` value:

* `player`

For `follow_target = player`, Carrot uses the scene's default `player_spawn_marker` as the authoring reference.

That means:

* the authored light position defines the offset from the default player spawn marker
* the runtime applies that offset relative to the live player object
* the chosen entry spawn does not change the meaning of the authored follow-light offset

This keeps follow lights stable across scenes with multiple valid player entry spawns.

If a scene authors no `Light` objects at all, the runtime keeps the default world-lighting state:

* ambient remains `1.0, 1.0, 1.0, 1.0`
* no authored point lights are created

#### Validation Expectations

Light validation is meant to help authors catch real contract mistakes.

Current important rules:

* missing or unrecognized `kind` warns
* `kind = point` without `radius` fails validation
* `behavior = follow` without `follow_target` fails validation
* `kind = ambient` with spatial or follow-only fields warns and ignores those fields
* unsupported `kind = spot` warns until spot-light import exists

The principle is:

* if a field is irrelevant because another field fully determines semantics, Carrot may ignore it with a warning
* if a chosen behavior is missing required data, Carrot fails validation instead of silently changing meaning


### Group Inheritance Is Supported, But Explicit

Tiled groups can pass inherited properties down to child layers.

Current important example:

* parent group `visibility_zone_id = inn_roof`
* child roof layer inherits that zone binding unless it overrides it directly

Carrot should rely on explicit properties for this, not group names.

---

## Validation Philosophy

Carrot should validate Tiled-authored data in a practical way:

* unsupported features should usually warn instead of failing import outright unless the map truly cannot function
* malformed authored contract usage should produce validation issues
* intentionally unused features should not warn just because a map does not use them

Examples:

* no `VisibilityZone` objects in a map is fine
* a layer that binds to a missing visibility zone id should warn
* an object with `visibility_zone_id` but the wrong `type` should warn

The goal is to help authors fix real mistakes without making the importer hostile.

---

## Animated Tiles

Carrot now supports Tiled-authored tileset animation metadata for inline TMJ tilesets.

Current behavior:

* animated frames are imported from Tiled's `tiles[].animation[]` data
* frame durations are interpreted in milliseconds, matching Tiled
* runtime rendering resolves the current animated tile frame from one engine-side frame clock value
* per-tile rendering reuses prebuilt animation lookup tables instead of reparsing animation data at draw time

This is intended to keep animated tiles practical for common cases like water and ambient map detail without turning tile rendering into a per-frame data-processing path.

---

## Practical Authoring Examples

### Roof Visibility By Authored Zone

Use this pattern when a roof or canopy should hide while the player is inside a specific authored area.

Author it in Tiled like this:

* create one or more rectangle objects with `type = VisibilityZone`
* give each object `visibility_zone_id = inn_roof`
* set each roof tile layer's Tiled `Class` to `Roof`
* put the roof tile layers under a group with `visibility_zone_id = inn_roof`
* optionally override a child layer directly with `carrot_visibility_zone` if one layer needs a different binding

Important behavior:

* multiple `VisibilityZone` objects may share the same `visibility_zone_id`
* this is the recommended way to cover an irregularly shaped building interior
* if no `VisibilityZone` objects exist in a map, the feature simply does nothing

### Conditional Front Tile Layers

Use this pattern when a tile layer should sometimes render in front of the actor and sometimes behind, based on actor position.

Good fits include:

* bridge rails
* fence fronts
* wall fronts
* counters

Author it in Tiled like this:

* keep the walkable floor or base tiles on a normal tile layer
* put the front-facing occluding tiles on a separate layer
* set `carrot_conditional_front = true` on that front layer

This keeps floor surfaces from drawing over the actor while still letting the front edge behave like an authored occluder.

### Always-Front Tile Layers

Use this pattern when a tile layer should always render above actors.

Good fits include:

* pure foreground dressing
* top canopy accents
* decorative overhang detail that should never sort behind the player

Author it in Tiled like this:

* place the tiles on their own layer
* set `carrot_always_front = true`

If both `carrot_always_front` and `carrot_conditional_front` are authored, always-front should be treated as the stronger intent and the map should be cleaned up.

### Same-Map Interior Front Layers

Use this pattern when a building stays on the same map and some indoor layers should only appear while the player is inside.

Good fits include:

* interior wall fronts
* shop counters
* indoor hanging trim
* room overlays that should stay above the player only while indoors

Author it in Tiled like this:

* author the roof layers with `Class = Roof`
* bind the roof and the indoor front layer to the same `visibility_zone_id`
* set the indoor front layer to `carrot_always_front = true`
* set the indoor front layer to `carrot_visibility_rule = visible_when_tag_active`

This creates the intended pairing:

* the roof hides while inside the zone
* the indoor front layer becomes visible while inside the zone
* that indoor front layer still renders above the player while active

### Typed Interaction Objects

Use typed objects when the object has stable engine-facing meaning rather than being just a generic marker.

Current examples:

* `type = Sign`
  * requires `message_id`
* `type = Container`
  * requires `loot_table`
* `type = Door`
  * requires `target_marker`
  * requires one of `target_scene` or legacy `target_map`
* `type = Trigger`
  * requires `trigger_id`
  * requires `trigger_kind`

For project-specific object types, it is fine to define a custom `type` and custom properties as long as game-side code owns the meaning.

---

## Current Validation Coverage

Current first-pass validation now checks for cases such as:

* `VisibilityZone` objects missing `visibility_zone_id`
* `VisibilityZone` objects authored as tile objects instead of rectangle objects
* zero-size `VisibilityZone` rectangles
* objects with `visibility_zone_id` but a non-`VisibilityZone` type
* layers that bind to visibility zone ids with no matching `VisibilityZone`
* conflicting front-policy flags on the same layer
* conflicting explicit/inherited visibility-zone bindings on the same layer

These issues are currently stored on imported tilemap assets and also logged during import.

---

## Object Geometry Coverage

Carrot now preserves basic Tiled object geometry intent on imported objects.

Current imported geometry kinds:

* rectangle
* point
* ellipse
* polygon
* polyline
* text

Current practical runtime usage is still narrower than the imported metadata:

* point objects work well for authored markers and spawn points
* rectangle objects remain the current primary shape for triggers and visibility zones
* polygon/polyline/ellipse geometry is preserved as object metadata for future runtime use, validation, and tooling
* text objects are still not treated as a supported gameplay/runtime feature

This is intentional.

Ticket 3 is widening the useful authored-data surface first, without pretending every imported shape already has full runtime semantics.

---

## Recommended Authoring Direction

When adding new Tiled-backed engine features, prefer:

* explicit object `type`
* explicit documented custom properties
* importer/runtime validation alongside the new feature
* narrow support for high-value Tiled features before broad speculative support

This keeps Tiled as a strong first-class workflow for Carrot without turning the engine contract into guesswork.

It also keeps the engine boundary clear:

* use engine-owned typed conventions for common shared world semantics
* use unknown/custom object `type` values for project-specific behavior until a broader engine contract is truly warranted
