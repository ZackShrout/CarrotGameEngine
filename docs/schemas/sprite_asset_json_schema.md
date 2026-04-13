# Carrot Game Engine – Sprite Asset JSON Schema

**Status:** Current working schema
**Applies to:** `*.sprite.json`
**Last Updated:** April 13, 2026

---

## Purpose

Sprite asset manifests define the engine-facing identity of a sprite plus the authored source the sprite importer should consume.

These files describe:

* stable sprite asset ids
* the texture asset backing the sprite
* the authored source file to import
* optional pivot and pixels-per-unit overrides

They do not directly store the full runtime frame/animation tables used by the engine at runtime.

For broader context, see:

* [asset_pipeline.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/asset_pipeline.md)
* [json_spec.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/json_spec.md)

---

## Recommended Example

```json
{
  "version": 1,
  "id": "sprite.vraden",
  "texture": "texture.vraden_sprite",
  "source": "game://sprites/vraden.aseprite.json",
  "pivot": { "x": 0.5, "y": 0.5 },
  "pixels_per_unit": 16
}
```

---

## Required Fields

### `id`

**Type:** `string`

Stable engine-facing logical asset id for the sprite.

### `texture`

**Type:** `string`

Logical texture asset id used by the imported sprite.

Rules:

* must be a valid logical asset id
* should refer to a texture asset manifest, not a raw file path

### `source`

**Type:** `string`

VFS path to the authored sprite source consumed by the importer.

Current supported source formats:

* `.aseprite.json`
* `.csprite.json`

The manifest points to authored/import source data, not to the runtime `.csprite` cooked artifact.

---

## Optional Fields

### `version`

**Type:** `integer`
**Default:** `1`

Current supported value is `1`.

### `pivot`

**Type:** `object`

Optional default pivot override applied during import.

Shape:

```json
{ "x": 0.5, "y": 0.5 }
```

### `pixels_per_unit`

**Type:** `number`

Optional pixels-per-unit override applied during import.

---

## Validation Notes

Import fails when:

* `id`, `texture`, or `source` is missing
* `id` is invalid or duplicated
* `texture` is not a valid logical asset id
* the source file does not exist
* the source extension is unsupported
* the schema version is unsupported

Import also fails if the underlying sprite source cannot be parsed or finalized into a valid sprite definition.

---

## Runtime Relationship

The authored manifest supplies the identity and import settings.

At runtime, the loader may prefer a valid cooked `.csprite` artifact, but that cooked artifact still references the sprite’s texture by logical asset id rather than duplicating texture pixels.
