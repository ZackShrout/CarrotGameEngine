# Carrot Game Engine - Milestone 13

**Last Updated:** April 13, 2026  
**Title:** Runtime Iteration and Editor Skeleton Foundation  
**Status:** Planned  
**Focus:** Introduce engine-owned runtime iteration and diagnostics services, then validate them through a thin optional `CarrotEditor` host.

---

## Milestone Goal

Milestone 12 gave Carrot a real imported/cooked pipeline:

* engine-owned derived artifacts
* deterministic invalidation inputs
* load-or-regenerate behavior
* separation between authored JSON and runtime data

That foundation is strong enough now that the next real bottleneck is daily iteration friction.

Milestone 13 exists to improve:

* how quickly content changes can be validated
* how clearly asset reload/regeneration behavior can be understood
* how safely runtime/tooling seams can grow

This milestone is successful if Carrot becomes easier to iterate on without making the engine editor-dependent.

For the system boundary this milestone should follow, see:

* [runtime_iteration_and_tooling.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/runtime_iteration_and_tooling.md)
* [asset_pipeline.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/asset_pipeline.md)
* [ARCHITECTURE_NOTES.md](/Users/zshrout/dev/CarrotGameEngine/docs/ARCHITECTURE_NOTES.md)

---

## Scope Summary

Milestone 13 is:

* a runtime iteration milestone
* a diagnostics milestone
* a tooling-boundary validation milestone
* a thin editor host milestone

Milestone 13 is not:

* a full editor milestone
* a scene/world editing milestone
* a project-management milestone
* a docking/layout framework milestone
* a replacement for Tiled or Aseprite
* a shift to editor-required workflows

---

## Core Architectural Rule

The engine owns runtime iteration.

The editor may:

* observe
* display
* trigger
* preview

The editor does not own:

* invalidation
* regeneration
* reload policy
* engine runtime state

If a feature only works when `CarrotEditor` is present, it does not belong in this milestone.

---

## Primary Deliverables

### 1. Engine-Owned Runtime Iteration Service

The engine should gain explicit runtime support for:

* detecting relevant authored-input or dependency changes
* invalidating affected runtime/cooked asset state
* regenerating cooked artifacts where applicable
* surfacing the result in structured diagnostics, not only logs

This should be built as engine/runtime functionality that both `CarrotSandbox` and `CarrotEditor` can use.

### 2. Asset Status and Diagnostics Surface

The engine should expose enough state to answer:

* did this asset load from cooked data or regenerate
* why was it invalidated
* which dependency changed
* when was the last reload attempt
* did the reload succeed, fail, or fall back

This may begin as a small query API or service rather than a huge generalized diagnostics framework.

### 3. Targeted Asset Reload Behavior

Reload should be implemented for a narrow, useful slice first.

Milestone 13 should prioritize:

* textures
* sprites
* audio assets
* shaders where backend/runtime safety is clear

Tilemaps and fonts may participate only if their reload behavior stays explicitly safe and understandable.

### 4. Thin Optional `CarrotEditor` Host

Introduce a separate executable target:

* `CarrotEditor`

It should:

* build on top of `CarrotEngine`
* target engine assets plus `CarrotSandbox` assets
* remain optional for development
* consume engine-facing APIs rather than privileged private hooks

### 5. Minimal Tooling Surfaces

The first editor/tooling surfaces should stay intentionally small.

Required minimum surfaces:

* asset list or browser
* diagnostics/status panel
* manual refresh or reload trigger
* at least one useful preview surface

Preview candidates:

* textures
* sprites
* tilemaps
* audio playback

Only the smallest genuinely useful set should ship in this milestone.

---

## Required Minimum Slice

To keep the milestone sharp, the minimum acceptable implementation should be:

1. engine-side asset reload/invalidation plumbing for textures, sprites, and audio
2. structured asset status/diagnostics queries
3. manual reload trigger path
4. `CarrotEditor` executable that can list assets and show diagnostics
5. one preview surface, plus one additional preview only if it comes cheaply

Automatic file watching is valuable, but it is not required before the manual reload path is solid.

---

## Reload Safety Rules

This milestone must not imply that all live runtime data hot reloads seamlessly.

Reload behavior should be explicit per asset type.

### Early Safe Targets

* textures
* sprites
* non-streaming audio assets
* selected shader resources where backend/runtime rules are well understood

### Cautious Targets

* tilemaps already backing a live scene
* scene assets that would require world rebuild
* fonts that may invalidate text layout assumptions mid-frame
* streamed audio assets if reload semantics are ambiguous

### Required Rule

Each participating asset type should clearly fall into one of these categories:

* safe live reload
* reload on next use
* manual refresh only
* requires restart or scene rebuild

The milestone should prefer honest, narrow behavior over magical claims.

---

## Implementation Boundaries

Milestone 13 should not introduce:

* editor-only asset formats
* hidden editor-owned registries
* private content models that bypass manifest/import/load paths
* a full inspector/property-editing framework
* world editing UI
* project creation workflows
* generalized docking/workspace architecture

The purpose here is to validate seams, not to design the entire future editor in one pass.

---

## Suggested Work Order

1. Add engine-side asset diagnostics/status data structures and query path.
2. Add manual refresh/reload flow for the first safe asset classes.
3. Add file-change detection only after the manual path is trustworthy.
4. Introduce `CarrotEditor` as a thin executable using those engine seams.
5. Add one preview surface and one diagnostics panel.
6. Expand to additional asset types only if the earlier slice stays clean.

This order keeps the engine boundary honest and prevents UI work from hiding missing runtime architecture.

---

## Non-Goals

To prevent scope creep, this milestone must not:

* implement scene/world editing
* add project creation or project switching workflows
* add prefab/archetype systems
* add a broad inspector/property-grid framework
* replace external content tools
* promise universal hot reload across every live runtime system

---

## Closeout Criteria

Milestone 13 is complete when:

* `CarrotEditor` builds and runs as a separate executable
* engine-owned runtime reload works for the first targeted asset slice
* asset status and diagnostics can be queried without reading raw logs
* the editor can inspect engine and sandbox assets through engine-facing APIs
* at least one asset preview surface is useful and stable
* runtime behavior remains correct when the editor is not running
* no engine system depends on `CarrotEditor`

---

## Summary

Milestone 13 is not “build the editor.”

It is:

* make runtime iteration meaningfully better
* make the asset pipeline understandable during development
* validate a thin editor host without surrendering engine ownership boundaries

If those three things are achieved cleanly, the milestone will have done its job.
