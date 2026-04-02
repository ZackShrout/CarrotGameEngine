# Carrot Game Engine – JSON Specification

**BunnySoft**
**System Design Document**
**Last Updated: March 2026**

---

## 1. Purpose

Carrot uses **JSON** as a primary format for human-authored structured data in the engine.

This document defines:

* what JSON is used for in Carrot
* what JSON is *not* used for
* the design philosophy behind Carrot’s JSON usage
* the conventions engine-owned JSON files should follow

This document exists to keep JSON usage in Carrot:

* intentional
* consistent
* understandable
* tool-friendly
* bounded

It should be understandable to:

* engine contributors
* engine users
* tooling/editor work
* AI/code assistants interacting with the codebase

This is not intended to be a universal serialization document for every possible system forever.
It is the current architectural policy for how JSON is used in Carrot.

---

## 2. Why Carrot Uses JSON

Carrot uses JSON because it is a strong fit for **authored structured metadata**.

At this stage of the engine, JSON provides a good balance of:

* readability
* portability
* diff-friendliness
* tooling compatibility
* simplicity

JSON works especially well for:

* asset definitions
* engine configuration
* content metadata
* imported external tool metadata
* editor-friendly structured authoring

This makes JSON a good fit for Carrot’s current content and tooling direction.

---

## 3. What JSON Is For in Carrot

Carrot uses JSON primarily for **human-authored or tool-authored structured data** that is:

* relatively small
* inspectable
* versionable
* meaningful in source control

### Common Intended Uses

Examples include:

* engine configuration
* audio asset definitions
* texture asset definitions
* future sprite asset definitions
* future tile map metadata
* future imported content descriptors
* future editor-facing authored metadata

### Key Principle

JSON in Carrot is intended to represent:

> **authored intent**

—not hot-path runtime state.

That distinction matters.

---

## 4. What JSON Is Not For in Carrot

JSON should **not** become Carrot’s universal answer to every data problem.

There are several things JSON is intentionally **not** for.

### JSON Is Not Intended For:

* runtime hot-path asset access
* performance-critical in-memory data structures
* large binary payloads
* cooked/cache-heavy derived asset formats
* arbitrary dynamic scripting logic
* “just serialize the whole engine object graph” workflows

### Important Rule

If a system starts needing:

* fast binary loading
* compact derived data
* runtime-efficient machine-owned representation
* platform-specific preprocessed content

…that may be a sign it should **stop being plain JSON** and become something else.

JSON should be used where it is a good fit, not forced where it is not.

---

## 5. Categories of JSON in Carrot

Carrot’s JSON usage falls into a few important categories.

Keeping these categories distinct helps avoid architectural confusion.

---

## 6. Engine Configuration JSON

These files define engine or application configuration.

Examples:

* graphics API preference
* debug layer settings
* audio settings
* future user/project config values

Example:

```json
{
  "version": 1,
  "graphics": {
    "api": "default",
    "debug_layers": "default"
  },
  "audio": {
    "sample_rate": 48000,
    "block_size": 512,
    "channels": 2
  }
}
```

### Design Intent

Configuration JSON should remain:

* explicit
* minimal
* stable
* easy to read and edit

Config JSON is not the same thing as asset metadata, even though both may use JSON.

---

## 7. Asset Definition JSON

These files define engine-owned asset metadata.

Examples include:

* `*.audio.json`
* `*.texture.json`
* `*.sprite.json`
* `*.tilemap.json`

These files define things such as:

* asset ID
* source file(s)
* import settings
* authored defaults
* engine-facing interpretation rules

Example:

```json
{
  "id": "music.oak_battle_theme",
  "source": "engine://audio/New RPG Battle Theme 3 Limit.wav",
  "bus": "music",
  "gain": 1.0,
  "looping": true,
  "loop_start": 3374900,
  "loop_end": 6666100,
  "spatial": "none",
  "streamed": true
}
```

### Design Intent

Asset definition JSON should describe:

> what an asset **means**

—not what the runtime object layout should literally be.

For more detail, see `docs/systems/asset_pipeline.md`.

---

## 8. External Tool JSON

Some JSON consumed by Carrot will originate from external tools rather than from Carrot itself.

Examples include:

* **Aseprite** exports
* **Tiled** exports
* future third-party tool metadata

This category is important because Carrot should treat it differently from engine-owned JSON.

### Important Rule

External tool JSON is not automatically “Carrot-native JSON.”

It should generally be treated as:

* imported source data
* external format input
* something Carrot interprets through import logic

—not as the engine’s own canonical internal metadata format.

### Why This Matters

This prevents the engine from accidentally letting third-party tool formats dictate its entire internal architecture.

Carrot should integrate with strong tools well, while still maintaining its own clear internal data model.

For example, Tiled object layers may currently author:

* invisible markers such as `PlayerSpawn` or `ExitNorth`
* visible placed tile objects on layers such as `props`
* hybrid objects that combine visual placement with semantic identity through:
  * object `type`
  * typed custom properties like `interactable`, `loot_table`, `target_map`, or `message_id`

---

## 9. JSON Philosophy in Carrot

Carrot’s JSON usage is guided by a few important principles.

### 9.1 Human-Readable by Default

Engine-owned JSON should be written for readability first.

That means:

* sensible field names
* clear structure
* no unnecessary compacting/minification in source-controlled authored files

### 9.2 Explicit Over Clever

Carrot JSON should prefer:

* explicit field names
* obvious structure
* stable meanings

over:

* cryptic shorthand
* clever polymorphic schema tricks
* unnecessarily dynamic structures

### 9.3 Fail Clearly

Carrot should prefer **clear validation and explicit failure** over silently accepting malformed or ambiguous data.

Bad data should fail loudly and informatively.

### 9.4 Engine-Owned JSON Should Be Stable

Engine-owned JSON formats should evolve intentionally, not casually drift every few weeks without structure.

This is one reason schema-like thinking matters even when a file is still “just JSON.”

---

## 10. Parsing and Validation Expectations

Carrot should treat JSON parsing as part of its **import/config boundary**, not as a runtime free-for-all.

### Expected Behavior

When Carrot parses engine-owned JSON, it should aim to:

* validate required fields
* apply sensible defaults where appropriate
* reject invalid types
* reject malformed required structure
* log useful, actionable errors

### Good Validation Behavior

Good validation means the engine should help answer:

* what file failed?
* what field is missing or invalid?
* what was expected?
* what should the user fix?

### Bad Validation Behavior

Bad validation would be:

* silent fallback to nonsense values
* “best effort” parsing that hides malformed data
* runtime systems discovering JSON problems too late

Validation should happen early and clearly.

---

## 11. Versioning and Format Evolution

Some JSON formats in Carrot may eventually need versioning.

That is normal.

### Good Use Cases for Versioning

Version fields are useful when:

* a config format may evolve over time
* a schema may need migration support
* an imported format may need compatibility handling

### Example

A config file may reasonably include:

```json
{
  "version": 1
}
```

### Important Rule

Versioning should be introduced where it is useful, but not every tiny JSON file needs to become a mini database migration system immediately.

Use it where it adds clarity and future safety.

---

## 12. Naming and Field Conventions

Carrot’s JSON should follow consistent conventions wherever practical.

### Recommended Conventions

* use **snake_case** for field names
* prefer explicit booleans over encoded magic strings
* prefer stable enums represented as readable strings
* avoid deeply nested structures unless they improve clarity
* prefer one clear meaning per field

### Example

Good:

```json
{
  "streamed": true,
  "looping": false,
  "spatial": "none"
}
```

Less desirable:

```json
{
  "mode": "stream_loop_none_01"
}
```

The goal is for Carrot JSON to remain understandable at a glance.

---

## 13. Relationship to the Asset Pipeline

JSON is a major part of Carrot’s asset pipeline, but it is **not the asset pipeline itself**.

This distinction matters.

### JSON’s Role

JSON is primarily used to express:

* authored asset metadata
* import-time intent
* engine-facing structured content definitions

### What Happens Next

After JSON is parsed and validated, the asset pipeline is responsible for turning that authored data into:

* imported metadata
* runtime loaded assets
* future optional cooked/cache layers where appropriate

That means JSON should stay on the **authoring/import side** of the boundary, not bleed into hot-path runtime systems.

For more detail, see `docs/systems/asset_pipeline.md`.

---

## 14. Relationship to Future Custom Formats

Carrot may eventually introduce engine-native custom formats such as:

* `.ctex`
* `.caud`
* other future cooked/cache formats

These would exist **alongside** JSON, not replace it entirely.

### Important Rule

Custom engine-native formats should exist to solve real problems such as:

* repeated import cost
* startup/load cost
* platform-specific derived data
* packaging/runtime simplification

They should not exist merely to replace human-readable authored metadata.

### Intended Relationship

A good long-term model is often:

* **JSON** for authored intent
* **engine-native derived formats** for machine-owned optimized outputs

That is a healthy split.

---

## 15. Relationship to Saves and Runtime Persistence

JSON may or may not be appropriate for some future save or persistence use cases, but that is a **separate design problem**.

Carrot should avoid casually blurring together:

* config files
* authored asset metadata
* imported external tool data
* cooked/cache artifacts
* save data

These are different categories of data and should remain architecturally distinct even if some of them happen to use JSON.

That separation matters more than whether the file extension happens to match.

---

## 16. What This System Should Preserve

As Carrot evolves, its JSON usage should continue preserving a few key ideas:

* JSON is for authored structured data, not hot-path runtime behavior
* engine-owned JSON should remain human-readable and stable
* external tool JSON should be treated as imported source data, not canonical engine architecture
* validation and failure should be clear
* JSON should stay on the authoring/import side of the boundary
* custom engine-native formats should be introduced only when they solve a real problem

If those rules remain intact, Carrot can continue using JSON effectively without letting it become architectural clutter.

---

## 17. Summary

Carrot uses JSON because it is a strong fit for:

* authored metadata
* configuration
* content definitions
* tool-friendly structured data

That is its role.

JSON is **not** intended to be:

* the runtime asset format
* the hot-path engine data model
* the answer to every serialization problem

The goal is to keep JSON in Carrot:

* useful
* bounded
* consistent
* readable
* architecturally appropriate

## See Also

- `CARROT_MASTER_PLAN.md`
- `ARCHITECTURE_NOTES.md`
- `docs/systems/asset_pipeline.md`
- `docs/schemas/audio_asset_json_schema.md`
