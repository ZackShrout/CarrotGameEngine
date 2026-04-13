# Carrot Game Engine – Runtime Iteration and Tooling Boundary

**BunnySoft**
**System Design Document**
**Last Updated: April 13, 2026**

---

## 1. Purpose

This document defines the intended boundary between:

* engine-owned runtime iteration behavior
* diagnostics and inspection services
* optional tooling/editor surfaces built on top of those services

It exists to keep future reload and editor work useful without letting the engine collapse into editor-owned architecture.

---

## 2. Core Rule

Carrot runtime iteration is an **engine capability**.

Tooling may:

* observe
* display
* trigger
* inspect

Tooling may not become the owner of:

* asset invalidation
* cooked-artifact regeneration
* runtime reload policy
* engine state authority

---

## 3. Why This Matters Now

After the imported/cooked pipeline work in Milestone 12, the next real pain is not missing formats.

It is:

* slow iteration after asset changes
* opaque regeneration behavior
* weak runtime visibility
* overreliance on logs as the only inspection surface

That makes runtime iteration a system concern first and an editor concern second.

---

## 4. Engine-Owned Responsibilities

The engine should own:

* file/dependency change detection
* invalidation decisions
* cooked artifact regeneration
* runtime cache eviction and refresh behavior
* reload events/state transitions
* diagnostics data describing what happened and why

This ownership should live in runtime services and asset systems, not in `CarrotEditor`.

---

## 5. Tooling Responsibilities

An optional tooling host such as `CarrotEditor` should act as an engine client.

It should be able to:

* list known assets
* query asset status and diagnostics
* request reload/refresh
* preview supported asset types
* display recent invalidation/regeneration results

It should not:

* require private engine-only code paths
* introduce hidden data models that bypass manifests/importers/loaders
* become required to run the sandbox or load assets correctly

---

## 6. Reload Safety Model

Reload should be treated as asset-type-specific, not as a magical universal promise.

### Safe Early Targets

These are good first runtime reload targets:

* textures
* sprites
* audio assets
* shader resources where backend/runtime safety is clear

### More Cautious Targets

These need tighter policy before claiming seamless live reload:

* tilemaps that already back a live scene
* scene bootstrap assets that affect spawned world structure
* fonts that may invalidate layout or atlas assumptions mid-frame

### Rule

Reload policy should be explicit per asset type:

* `reloadable_live`
* `reloadable_on_next_use`
* `manual_refresh_only`
* `restart_or_scene_rebuild_required`

The engine should expose that policy rather than pretending everything hot reloads equally well.

---

## 7. Diagnostics Expectations

Runtime iteration work should make the asset pipeline understandable without tailing logs constantly.

The engine should expose enough data to answer:

* was this asset loaded from cooked data or regenerated
* why was it invalidated
* which dependency changed
* when did the last reload attempt happen
* did reload succeed, fail, or fall back

Logs still matter, but they should not be the only surface carrying this information.

---

## 8. Minimum Useful Tooling Host

A thin first-pass tooling host is useful when it validates real seams with minimal architecture.

A minimum useful `CarrotEditor` should provide:

* asset list or browser
* asset diagnostics/status panel
* manual refresh/reload controls
* one or more focused preview surfaces such as texture, sprite, tilemap, or audio preview

That is enough to prove:

* engine/editor separation
* usable diagnostics APIs
* practical iteration improvements

It is not yet a full editor architecture.

---

## 9. Non-Goals

This direction does not imply:

* project creation workflows
* docking/layout systems
* scene/world editing
* prefab/archetype editors
* replacement of Tiled or Aseprite
* editor-owned asset processing

---

## 10. Success Criteria

This system direction is succeeding when:

* runtime reload behavior works without any editor running
* diagnostics are queryable without scraping logs
* tooling uses public engine-facing seams instead of backdoors
* the editor can disappear without breaking the engine

That boundary is more important than how quickly the first tooling UI appears.
