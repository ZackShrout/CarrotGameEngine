# Carrot Game Engine – Tilemap Asset JSON Schema

**Status:** Current working schema
**Applies to:** `*.tilemap.json`
**Last Updated:** April 13, 2026

---

## Purpose

Tilemap asset manifests define the engine-facing identity of an authored tilemap source.

These files describe:

* stable tilemap asset ids
* the authored source file to import

They do not directly store the full runtime tilemap structure, imported object data, collision extraction, or cooked map payload.

For broader context, see:

* [asset_pipeline.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/asset_pipeline.md)
* [tiled_authored_data.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/tiled_authored_data.md)
* [json_spec.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/json_spec.md)

---

## Minimal Example

```json
{
  "id": "tilemap.sandbox.town",
  "source": "game://tilemaps/test_town.tmj"
}
```

---

## Supported Source Formats

Current supported source formats:

* `.tmj`
* `.ctilemap.json`

`.tmj` is treated as imported Tiled source data.
`.ctilemap.json` is treated as Carrot-native authored tilemap data.

The manifest points to authored/import source data, not to the runtime `.cmap` cooked artifact.

---

## Required Fields

### `id`

**Type:** `string`

Stable engine-facing logical asset id for the tilemap.

### `source`

**Type:** `string`

VFS path to the authored tilemap source.

Rules:

* should use `engine://` or `game://`
* must point to an existing source file
* must use a supported source format

---

## Optional Fields

### `version`

**Type:** `integer`
**Default:** `1`

Current supported value is `1`.

---

## Validation Notes

Import fails when:

* `id` or `source` is missing
* `id` is invalid or duplicated
* the source file does not exist
* the source extension is unsupported
* the schema version is unsupported
* the underlying tilemap source cannot be parsed or imported successfully

---

## Runtime Relationship

The authored tilemap manifest defines identity and source intent.

At runtime, the loader may prefer a valid cooked `.cmap` artifact, but the cooked map still keeps texture/tileset references at the asset/source level rather than embedding low-level texture payloads.
