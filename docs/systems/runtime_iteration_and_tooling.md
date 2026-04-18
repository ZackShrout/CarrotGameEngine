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
* focused preview surfaces for currently supported asset types
* runtime scene/object/system inspection surfaces

That is enough to prove:

* engine/editor separation
* usable diagnostics APIs
* practical iteration improvements

It is not yet a full editor architecture.

### Current Implemented Slice

The current repo already implements a thin first-pass tooling host with:

* asset list/status browsing
* manual reload controls
* texture preview
* sprite preview
* runtime scene summary inspection
* runtime object inspection
* runtime systems inspection

Near-term likely additions include:

* audio preview
* tilemap preview
* broader asset-type-specific inspection panels

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

## 11. Current Reload Scope

Current implementation scope is intentionally narrower than the full design envelope.

Today:

* textures support live runtime reload
* sprites support live runtime reload
* non-streamed audio supports live runtime reload
* streamed audio is tracked but remains manual-refresh-oriented
* fonts, tilemaps, and scenes are tracked but currently sit in scene-rebuild/restart-oriented territory

This is consistent with the intended safety model:

* simple leaf assets can be more aggressive
* structure-shaping assets should remain explicit until rebuild rules are clearer

## 12. Current Dependency Reasoning Model

The engine now exposes a first-pass dependency surface for runtime iteration status.

This is not yet a full dependency graph.
It is an engine-owned summary of what kind of dependency truth each asset currently carries and how the runtime treats it.

Current dependency shapes:

* `leaf_runtime_data`
  * texture assets
  * audio assets
  * these depend primarily on authored source bytes plus manifest/import settings
* `referenced_runtime_assets`
  * sprite assets
  * these depend on authored sprite data plus at least one referenced runtime asset such as a texture
* `layout_or_presentation_contract`
  * font assets
  * these affect text layout/presentation assumptions and remain rebuild-oriented today
* `scene_or_world_structure`
  * tilemap assets
  * scene assets
  * these affect active world composition, collision, layering, bindings, authored lights, or other structure-shaping runtime state

Current watch modes:

* `source_and_manifest_timestamps`
  * currently used for textures, sprites, and audio
  * this means the engine polls source/manifest write times and can attempt live runtime action when policy allows
* `not_polled`
  * currently used for fonts, tilemaps, and scenes
  * this does not mean the dependency shape is unknown
  * it means the engine is not yet polling that asset kind for automatic runtime timestamp-driven action

This distinction matters:

* dependency shape explains what the asset depends on
* watch mode explains how the engine currently observes change for automatic runtime iteration

That separation keeps the iteration model more honest:

* some assets are well-understood but intentionally conservative
* some assets are both well-understood and safe enough for live polling/reload

## 13. Current Diagnostics Surface

The engine now exposes a richer diagnostics surface for runtime iteration than just raw reload policy and last success/failure.

Current diagnostics include:

* `last_watch_change`
  * records whether the most recently observed watched change came from source timestamps, manifest timestamps, both, or neither
* `last_refresh_request_origin`
  * records whether the most recent explicit runtime refresh request came from a manual request or automatic watch polling
* `last_requested_action`
  * records the engine-owned runtime action that was most recently requested for that asset
* action-reason explanation
  * explains why the engine chose `reload_now`, `reload_on_next_use`, `manual_refresh`, `rebuild_current_scene`, or `restart_runtime`
* attempt summary
  * explains the last recorded runtime result in more useful terms than enums alone, such as whether the asset came from cooked cache, regenerated from source, streamed directly, or failed
* invalidation detail
  * explains why the cooked artifact was considered stale or missing when that information is available

This currently feeds two places:

* engine log messages during automatic watch-driven refresh
* `CarrotEditor` asset diagnostics/details panels

The goal is not to create a second ad hoc debug language.
The goal is for runtime/editor tooling to read the same engine-owned iteration explanation surfaces instead of inventing their own interpretations.

## 14. Structural Refresh Integration

Structural refresh is now tied more directly into scene runtime behavior than it was in the first iteration pass.

Today:

* structural or presentation-contract assets can still recommend `rebuild_current_scene`
* `CarrotEditor` no longer treats that as only a generic button label
* the rebuild path can now be requested as an asset-driven scene rebuild tied to a specific asset iteration status

That means the scene runtime can carry:

* which asset triggered the structural refresh request
* what kind of asset it was
* why the engine considered a scene rebuild the safest path

That context is surfaced through runtime transition diagnostics, not just editor-local UI state.

This is intentionally modest in scope:

* it does not make every structural asset live reloadable
* it does not promise background graph-driven rebuild automation
* it does make rebuild-oriented iteration more explicit, traceable, and engine-owned
