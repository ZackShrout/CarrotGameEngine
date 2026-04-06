# Carrot Game Engine - Milestone 06

**Last Updated:** April 5, 2026
**Title:** Richer Tiled Feature Coverage and Authored Data Support
**Status:** Complete
**Focus:** Expand Carrot's Tiled pipeline from a strong first-pass map workflow into a broader, cleaner, and more explicit authored-data foundation for real game content.

---

## Milestone Goal

Milestone 04 made Tiled-authored worlds playable.

Milestone 05 made those authored worlds render with a much stronger engine-owned layering model.

The next ceiling is not just one missing gameplay system.

It is the breadth and clarity of Carrot's authored-data contract with Tiled.

Right now Carrot has a good first slice:

* tilemaps render
* tileset collision imports
* triggers import
* visible tile objects import
* scene markers work
* group-aware layering metadata works

But there are still important gaps between "Carrot can load Tiled maps" and "Tiled is a strong first-class content workflow for Carrot."

Milestone 06 is about closing that gap.

This milestone should expand supported Tiled features, tighten the authored/runtime contract, and reduce the amount of special knowledge required to use Tiled correctly with the engine.

---

## Scope Summary

Milestone 06 is **not** just "support more random TMJ fields."

It is an **authored data pipeline and Tiled contract milestone**.

The intended near-term direction is:

* broader supported authored features first
* clearer authored/runtime rules second
* validation and documentation alongside implementation

That means this milestone should prioritize:

* stronger support for meaningful Tiled features Carrot is likely to rely on
* clearer typed authored conventions instead of loose stringly behavior where possible
* importer/runtime behavior that remains engine-owned and predictable
* validation and debugability around authored map data
* continued reduction of sandbox-only assumptions in map-driven gameplay data

It should not sprawl into:

* a full in-engine editor milestone
* arbitrary support for every Tiled feature regardless of Carrot relevance
* heavy bespoke content rules that belong in game code rather than engine boundaries
* replacing the scene/world architecture with a giant data-driven everything-system

---

## Why This Milestone Comes Next

The current roadmap order after Milestone 05 is:

1. gamepad input support
2. in-game UI foundation and API
3. richer Tiled feature coverage and authored data support

But in practice, stronger Tiled support is a very good next move right now:

* the Tiled workflow is already active and heavily used
* Milestones 04 and 05 exposed exactly where the current authored-data edges still are
* better authored-data support will make later gameplay, UI prompts, interaction content, and world polish easier to build
* the current engine already treats Tiled as a first-class workflow target, so deepening that contract is aligned with the architecture

This milestone turns Tiled from "good enough for current maps" into a stronger long-term authored content foundation.

---

## Current Implementation Baseline

Carrot already has a meaningful first-pass Tiled integration:

* tilemap asset discovery and manifest import
* Tiled-backed tilemap runtime data
* tile layer and object layer import
* tileset texture binding
* tileset rectangle collision import
* trigger rectangle import
* visible tile object import
* scene markers and spawn markers
* group-aware inherited properties for layering-related metadata
* first-pass layering, visibility-zone, and front-policy authored contracts

Current milestone progress already includes:

* a documented Tiled-authored data contract
* importer-side validation with stored authored-data issues
* tileset-defined animated tile import and runtime rendering support
* preserved object geometry kinds for rectangle, point, ellipse, polygon, polyline, and text
* stronger typed object conventions for `Sign`, `Container`, `Door`, `Trigger`, and `VisibilityZone`
* clearer authored guidance for layer class usage, visibility zones, conditional-front layers, and always-front layers

Current remaining pressure points:

* importer/runtime introspection can still be improved
* regression coverage should continue to deepen as new Tiled-backed features land
* the docs are much stronger now, but milestone closeout guidance still needs a final finish once Ticket 4 is complete
* richer shapes are preserved, but only some currently have strong runtime semantics

---

## Ticket 1 - Tiled Authored Data Contract and Validation

**Priority:** P0
**Outcome:** Carrot has a clearer, documented, and more enforceable authored-data contract for Tiled content.

### Why

Before broadening feature support too far, the engine needs a cleaner answer to:

* which Tiled features are intentionally supported
* which authored properties are part of Carrot's contract
* what validation should happen when content is malformed or incomplete

### Scope

Define and document the supported authored-data surface across:

* tile layers
* object layers
* groups
* objects
* tilesets
* scene-linked map semantics where appropriate

Likely outputs:

* a stronger Tiled-authored data doc
* explicit rules for supported object `type` usage
* clearer property naming conventions and precedence rules
* importer/runtime validation behavior for missing or conflicting authored data

### Acceptance Criteria

* Carrot has a documented Tiled-authored contract that goes beyond ad hoc examples.
* Unsupported or malformed authored data fails or warns intentionally where appropriate.
* The engine is clearer about what is a supported authored pattern versus a tolerated fallback.

---

## Ticket 2 - Animated Tiles and Richer Tile Runtime Support

**Priority:** P0
**Outcome:** Tiled-authored animated tiles become a real engine-supported content path.

### Why

Animated tiles are one of the most visible remaining gaps in Carrot's Tiled support.

They matter for:

* water
* torches
* signs
* ambient world detail
* generally making Tiled-authored spaces feel alive

### Scope

Add first-pass support for:

* tileset-defined animated tile metadata
* runtime frame progression for animated tiles
* renderer integration for animated tile frame selection
* sane timing behavior that does not require gameplay code hacks

The goal is not a giant animation framework.

It is a clean engine-owned path for the way Tiled already authors animated tiles.

### Acceptance Criteria

* A Tiled-authored animated tile renders as animated content in Carrot.
* Animation timing is engine-owned and deterministic enough to build on.
* Animated tiles integrate with the current tilemap rendering path cleanly.

---

## Ticket 3 - Richer Object, Shape, and Metadata Coverage

**Priority:** P0
**Outcome:** Carrot supports a broader useful subset of Tiled-authored objects and map semantics than the current rectangle-heavy first pass.

### Why

Milestone 04 intentionally focused on rectangles and narrow gameplay needs.

That was the right first step, but real authored content will likely need more flexibility.

### Scope

Expand support where it fits Carrot's gameplay-first model, potentially including:

* broader object shape handling where clearly useful
* stronger typed object conventions
* richer per-object metadata import
* clearer import of group/object visibility and inheritance behavior
* continued support for hybrid object patterns driven by Tiled `type` plus custom properties

Typed object growth should stay disciplined:

* Carrot should keep a small built-in object type set for common engine-facing semantics
* obvious future candidates such as `SpawnPoint`, `NPC`, `Pickup`, `Switch`, and `PatrolPath` should only become built-in when real project needs justify the contract
* unknown object `type` values should continue to import cleanly as raw authored objects so game code can define project-specific semantics without engine bloat

This ticket should remain practical.

It does not need to become "support every Tiled construct equally."

### Acceptance Criteria

* Carrot supports a broader and more useful authored-object subset than it does today.
* The importer/runtime path stays understandable rather than turning into a bag of special cases.
* New supported patterns are documented as part of the authored-data contract.

---

## Ticket 4 - Better Import Introspection, Regression Coverage, and Authoring Guidance

**Priority:** P1
**Outcome:** Tiled support is easier to trust, debug, and extend.

### Why

As authored-data support grows, regressions and confusing content failures become more likely unless the engine can explain what it imported and why.

### Scope

Add practical support such as:

* targeted regression coverage around new Tiled features
* importer/runtime debug snapshots or inspection data where useful
* clearer authoring examples and docs
* focused validation maps or sandbox coverage for new supported patterns

### Acceptance Criteria

* New Tiled feature support comes with regression coverage.
* The engine provides enough debugability to reason about authored-data failures.
* Future Tiled support work becomes easier rather than harder to extend safely.

### Current Ticket 4 Direction

At this stage, Ticket 4 is less about inventing brand-new map semantics and more about making the current Tiled contract easier to trust and extend.

The practical finish line looks like:

* stronger docs with concrete authored examples
* milestone notes that reflect what has actually shipped
* enough importer/runtime inspection data and regression coverage to keep future Tiled work safe

---

## Non-Goals

Milestone 06 should **not** expand into:

* a full editor or map-authoring UX milestone
* full gamepad support
* the full in-game UI framework
* every exotic Tiled export feature regardless of game value
* a deep simulation or AI-authored-data milestone

---

## Success Criteria

Milestone 06 is successful if:

* Carrot supports a meaningfully broader and better-documented Tiled workflow than it does after Milestone 05.
* Animated tiles and other high-value authored features no longer feel like obvious gaps.
* Supported object/layer/group conventions are easier to understand and validate.
* The Tiled-to-Carrot pipeline feels more like a stable engine feature and less like a collection of first-pass import paths.

At this point, the remaining work is mostly Ticket 4 polish and milestone closeout rather than major missing Tiled feature breadth.

---

## Closeout Notes

Milestone 06 is complete.

Delivered outcomes:

* a much clearer Tiled-authored data contract
* importer-side authored-data validation with non-fatal diagnostics
* tileset-authored animated tile support
* stronger layer class and property conventions for engine-facing authored behavior
* preserved richer object geometry metadata
* a disciplined typed-object contract with documented room for future engine-owned types and game-specific custom types

Intentional deferrals:

* more typed object conventions should wait for real project pressure
* richer polygon/polyline/ellipse runtime meaning can come later
* deeper importer introspection can be expanded when debugging pressure justifies it

This milestone successfully moved Carrot from "Tiled map import works" toward "Tiled is a trustworthy first-class authored world workflow."

---

## Likely First Concrete Targets

If this milestone starts soon, the strongest first implementation targets are probably:

1. formalize the authored-data contract and validation rules
2. add animated tile import/runtime/render support
3. expand useful object/shape/metadata support based on the next real sandbox needs
4. add regression coverage and importer inspection support alongside each slice

That keeps the milestone grounded in high-value Tiled workflow improvements instead of turning into an unbounded importer rewrite.
