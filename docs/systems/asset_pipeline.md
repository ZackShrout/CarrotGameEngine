# Carrot Game Engine – Asset Pipeline

**BunnySoft**
**System Design Document**
**Last Updated: April 10, 2026**

---

## 1. Purpose

Carrot’s asset pipeline exists to define how engine and game content moves from:

* **authored source files**
* through **engine-owned metadata and import logic**
* into **runtime-usable engine assets**

The asset pipeline is designed to work:

* **without an editor today**
* **with an editor later**
* **with strong external tool workflows**
* **without sacrificing runtime clarity or performance**

This document describes how Carrot currently thinks about assets and how the asset system is intended to evolve.

It should be understandable to:

* engine contributors
* engine users
* future tooling/editor work
* AI/code assistants interacting with the codebase

This is not intended to be a low-level implementation spec.
It is the architectural model for how Carrot handles assets.

---

## 2. Core Goals

Carrot’s asset pipeline is built around a few key goals.

### 2.1 Assets Should Be Explicit

Assets should be understandable in source control and easy to inspect.

The engine should not rely on mysterious opaque content blobs just to represent normal authored content.

### 2.2 Runtime Systems Should Not Parse Authoring Data

Runtime systems should not be opening JSON files, parsing strings, or interpreting external tool formats during hot paths.

That work belongs to import/load steps, not gameplay or rendering hot loops.

### 2.3 Authoring Should Be Tool-Friendly

The asset pipeline should work well with:

* hand-authored metadata
* engine-owned JSON descriptors
* external content tools
* future editor workflows

### 2.4 The Same Asset Model Should Scale Over Time

Carrot’s asset model should work:

* in a small no-editor project
* in a larger project with stronger import workflows
* with future editor support
* with optional future cooked/cache layers

The pipeline should scale without needing to be reinvented every time the engine grows.

---

## 3. Core Asset Model

Carrot assets should always be thought of as **four distinct layers**.

This distinction is one of the most important architectural rules in the engine.

---

## 4. The Four Asset Layers

### 4.1 Source Files

These are the original authored media or source inputs.

Examples:

* `.png`
* `.wav`
* `.ttf`
* shader source files
* future sprite sheets
* future tile map exports
* future external tool exports

These are the *raw inputs* the engine works from.

They are not runtime assets by themselves.

---

### 4.2 Authored Asset Definitions

These are the engine-facing metadata files that tell Carrot:

* what an asset is
* what source file(s) it depends on
* how it should be interpreted
* what defaults or import settings apply

Examples:

* `*.texture.json`
* `*.audio.json`
* `*.sprite.json`
* `*.tilemap.json`
* `*.scene.json`
* future asset-adjacent metadata files

These files are:

* human-readable
* source-control-friendly
* easy to diff
* easy to validate
* editor-compatible later

These files define **intent**, not final runtime form.

---

### 4.3 Runtime Loaded Assets

These are the actual in-memory engine objects produced after import/load.

Examples:

* decoded image data
* loaded audio samples
* streaming audio state
* GPU texture resources
* loaded sprite definitions
* loaded tilemap runtime structures

These are the objects runtime systems actually use.

They are not raw JSON, and they are not just file paths.

---

### 4.4 Imported / Cooked / Cached Artifacts

These are machine-generated, engine-native derived outputs that Carrot is now expected to introduce as the next major asset-pipeline expansion.

Examples:

* `.ctex`
* `.caud`
* `.csprite`
* `.cmap`
* other platform- or importer-specific derived data

Carrot should introduce these artifacts only when they solve a real problem such as:

* startup cost
* repeated import cost
* packaging simplicity
* platform-specific derived data
* runtime simplification

Carrot should not introduce cooked formats merely to feel “engine-like.”

Important rule:

* authored JSON remains the source of truth
* imported/cooked artifacts remain machine-generated derived outputs
* runtime systems should prefer valid imported artifacts when available
* stale or missing imported artifacts should be regenerated automatically

---

## 5. Asset Identity

Every asset in Carrot should have a **stable human-facing asset ID**.

Examples:

* `music.oak_battle_theme`
* `engine.carrot_engine_logo_512`
* future examples like:

    * `sprite.player.walk_down`
    * `tilemap.overworld.route_1`

### Asset ID Goals

Asset IDs should be:

* stable
* readable
* source-control-friendly
* tool-friendly
* independent from raw file names where practical

### Important Rule

Game code and engine systems should prefer **asset IDs** and loaded asset access patterns over direct dependence on raw source file paths.

Paths are for the asset pipeline.

IDs are for engine-facing identity.

---

## 6. Virtual File System (VFS)

Carrot uses a **virtual file system** so asset references remain portable and explicit.

Current / planned virtual roots include:

* `engine://`
* `game://`
* `source://`
* `save://`

### Why This Exists

The VFS exists to solve several problems cleanly:

* engine assets vs game assets should remain clearly separated
* asset references should not depend on fragile absolute paths
* content should remain relocatable
* the engine should have a stable asset-addressing model

### Example

A texture asset definition might reference:

```json
{
  "id": "engine.carrot_engine_logo_512",
  "source": "engine://images/carrot_engine_logo_512.png",
  "srgb": true
}
```

That lets the engine resolve the asset through the VFS rather than through platform-specific path assumptions.

---

## 7. Why Carrot Uses JSON

Carrot currently uses JSON for authored asset definitions because it is a strong fit for this stage of the engine.

### JSON Is Good For:

* authored metadata
* configuration
* human-readable asset definitions
* external tool interoperability
* diff-friendly source control workflows

### JSON Is Not For:

* runtime hot-path access
* large opaque binary payloads
* performance-critical gameplay data access
* permanent runtime in-memory asset representation

This distinction matters.

Carrot is not a “JSON-driven runtime engine.”
Carrot uses JSON as an **authoring and import format**, not as a runtime system architecture.

---

## 8. Importer vs Loader Split

This is one of the most important concepts in the asset pipeline.

The engine should preserve this distinction clearly.

---

## 9. Importers

Importers are responsible for understanding **authored asset definitions**.

Their job is to answer questions like:

* what asset type is this?
* what is its asset ID?
* what source file(s) does it depend on?
* what authored settings apply?
* what defaults need to be applied?
* is the authored metadata valid?

### Importers Should:

* parse engine-owned metadata
* validate required fields
* resolve referenced source files
* normalize authored values
* register or prepare asset metadata

### Importers Should Not:

* become general runtime systems
* own hot-path runtime behavior
* leak authoring concerns into gameplay/rendering/audio code

Importers understand **intent**.

---

## 10. Loaders

Loaders are responsible for producing **runtime-usable asset data**.

Their job is to answer questions like:

* decode the PNG
* load or stream the WAV
* resample audio if needed
* construct runtime image/audio/sprite/tilemap data
* create GPU-facing resources when appropriate

### Loaders Should:

* transform imported asset data into runtime-usable objects
* handle actual loading/decoding/creation work
* return stable engine-facing loaded asset representations

### Loaders Should Not:

* act like schema parsers
* become a dumping ground for arbitrary authoring logic
* collapse all import concerns into runtime load code

Loaders understand **execution**.

---

## 11. Current Asset Types in Carrot

Carrot’s current asset system is still growing, but the pattern is already established.

### 11.1 Audio Assets

Audio assets currently use authored metadata such as:

* `*.audio.json`

These define things like:

* asset ID
* source audio file
* bus
* gain
* looping behavior
* spatial behavior
* streaming behavior

At runtime, these become engine-facing audio asset structures and playback-ready runtime objects.

For more detail on runtime audio architecture, see `docs/systems/audio_engine.md`.

### 11.2 Texture Assets

Texture assets currently use authored metadata such as:

* `*.texture.json`

These define things like:

* asset ID
* source image file
* texture interpretation settings
* color space intent (e.g. sRGB)

At runtime, these become image data and/or GPU texture-facing runtime structures.

### 11.3 Sprite Assets

Sprite assets use authored metadata such as:

* `*.sprite.json`

These define sprite-facing runtime content such as:

* sprite asset id
* texture dependency
* frame layout
* animation-facing metadata
* `pixels_per_unit`

At runtime, these become loaded sprite definitions used by world objects, animation systems, and renderer-facing draw code.

### 11.4 Tilemap Assets

Tilemap assets use authored metadata such as:

* `*.tilemap.json`

These define things like:

* tilemap asset id
* source Tiled export path
* importer/runtime interpretation settings

At runtime, these become tilemap runtime structures that can drive both tile rendering and tile/object-layer world import.

### 11.5 Scene Assets

Scene assets use authored metadata such as:

* `*.scene.json`

These define higher-level bootstrap intent such as:

* scene asset id
* primary tilemap asset id
* default player sprite and spawn marker
* initial music
* scene-level bootstrap defaults

At runtime, these give Carrot a reusable scene-loading path instead of requiring sandbox-local world assembly code for every playable map.

### 11.6 Future Asset Types

The same asset model is intended to scale to future asset types such as:

* sprite animations
* fonts
* materials
* other engine-owned content types

The important thing is not the specific asset type.

The important thing is that each type fits the same **authoring → import → load → runtime** mental model.

---

## 12. External Content Tool Workflows

Carrot is intentionally designed to work well with strong external content tools rather than requiring a custom editor immediately.

This is a major design decision, not a temporary hack.

### 12.1 Aseprite

Aseprite is intended to be a first-class sprite workflow tool for Carrot.

Planned uses include:

* sprite sheet creation
* frame-based animation
* tag-based animation workflows
* exported animation metadata

Carrot should consume Aseprite-friendly exports cleanly rather than forcing users into an engine-specific sprite workflow too early.

### 12.2 Tiled

Tiled is intended to be a first-class tile map workflow tool for Carrot.

Planned uses include:

* tile layers
* object layers
* visible placed props via tile objects
* gameplay markers and authored scene anchors
* hybrid objects using semantic type + custom properties
* animated tiles
* tilesets
* map metadata
* larger world composition workflows

This includes support for serious Tiled workflows such as:

* multi-map world composition
* streaming-style overworld structures
* larger zone/world layouts

The current scene layer now sits above tilemaps rather than replacing them.
Tiled remains the environment authoring tool, while `*.scene.json` provides the engine-facing bootstrap and transition context around those authored maps.

### 12.3 Why This Matters

Carrot should prefer:

* integrating with strong existing tools well

over:

* reinventing weak custom tools too early

That is an intentional architectural choice.

---

## 13. Runtime Asset Philosophy

Runtime asset access should be:

* explicit
* predictable
* low-overhead
* free of authoring-format concerns

### Important Runtime Rule

Runtime systems should not be doing things like:

* opening JSON files during gameplay
* parsing asset metadata during draw/update hot paths
* resolving tool-export semantics in real-time
* repeatedly reinterpreting authored content

That work belongs before or during load, not during gameplay hot paths.

### Desired Runtime Shape

The runtime-facing side of the asset system should trend toward:

* stable loaded asset access
* low-overhead retrieval
* explicit ownership/lifetime
* minimal surprises

The exact internal registry/handle model may continue evolving, but these goals should remain constant.

---

## 14. Mutability and Replacement

As a general architectural rule, assets should be treated as **effectively immutable runtime content**.

That means:

* assets are loaded into stable runtime representations
* asset definitions are not mutated casually during gameplay
* content changes should happen through replacement/reload rather than ad hoc mutation

This model makes the asset system:

* easier to reason about
* friendlier to hot reload
* safer for multi-system usage
* easier to integrate with future tooling

This does **not** mean every runtime object in the engine must be literally immutable in every implementation detail.

It means the **asset model** should prefer replacement over in-place authoring-style mutation.

---

## 15. Startup Import vs Future Offline Cooking

Carrot currently leans toward a **startup import / startup load** model for many asset types.

That is completely acceptable at this stage of the engine.

### Current Strengths of This Model

* simple
* understandable
* easy to debug
* easy to iterate on
* editor-independent

### Why This May Change Later

As the engine grows, some asset types may eventually benefit from:

* offline preprocessing
* cached derived data
* platform-specific cooked outputs
* faster startup/load workflows

That is a future optimization and tooling concern.

It should be introduced when it becomes worthwhile, not before.

---

## 16. What This System Should Preserve

As Carrot evolves, the asset pipeline should continue preserving a few key ideas:

* source files are not the same thing as runtime assets
* authored metadata is not runtime data
* JSON is an authoring/import format, not a runtime architecture
* VFS-backed asset addressing matters
* importer vs loader separation matters
* external tool interoperability matters
* cooked formats should be earned, not assumed
* runtime systems should stay free of authoring-format concerns

If those rules remain intact, Carrot’s asset system can evolve significantly without losing clarity.

---

## 17. Summary

Carrot’s asset pipeline is intended to provide a clean, scalable path from:

**source content**
→ **authored metadata**
→ **imported intent**
→ **runtime-usable assets**

That model works:

* today without an editor
* later with an editor
* with external tools
* with future cooked/cache layers if they become worthwhile

That is the point of the system.

The goal is not just to “load files.”

The goal is to make asset handling in Carrot:

* coherent
* predictable
* tool-friendly
* runtime-safe
* future-ready

## See Also

- `CARROT_MASTER_PLAN.md`
- `ARCHITECTURE_NOTES.md`
- `docs/systems/json_spec.md`
- `docs/systems/audio_engine.md`
- `docs/schemas/audio_asset_json_schema.md`
