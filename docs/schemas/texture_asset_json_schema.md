# Carrot Game Engine – Texture Asset JSON Schema

**Status:** Current working schema
**Applies to:** `*.texture.json`
**Last Updated:** April 13, 2026

---

## Purpose

Texture asset manifests define the engine-facing identity and import settings for authored texture sources.

These files describe:

* stable texture asset ids
* source image location
* import-time color-space intent

They do not store runtime GPU texture state or cooked texture payloads.

For broader context, see:

* [asset_pipeline.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/asset_pipeline.md)
* [json_spec.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/json_spec.md)

---

## Minimal Example

```json
{
  "id": "engine.carrot_engine_logo_512",
  "source": "engine://images/carrot_engine_logo_512.png"
}
```

---

## Recommended Example

```json
{
  "version": 1,
  "id": "engine.carrot_engine_logo_512",
  "source": "engine://images/carrot_engine_logo_512.png",
  "srgb": true
}
```

---

## Required Fields

### `id`

**Type:** `string`

Stable engine-facing logical asset id.

Rules:

* must be unique in the texture asset space
* should follow Carrot logical asset id rules
* should stay stable even if the source filename changes

### `source`

**Type:** `string`

VFS path to the authored source image.

Rules:

* should use `engine://` or `game://`
* must point to an existing source image
* should not point to cooked artifacts like `.ctex`

---

## Optional Fields

### `version`

**Type:** `integer`
**Default:** `1`

Current supported value is `1`.

### `srgb`

**Type:** `boolean`
**Default:** `true`

Controls whether the texture should be imported as sRGB content.

Typical guidance:

* `true` for color/albedo/UI art
* `false` for data textures where numeric accuracy matters

---

## Validation Notes

Import fails when:

* `id` is missing or invalid
* `source` is missing
* the source file does not exist
* the schema version is unsupported
* the asset id collides with an existing texture asset

---

## Runtime Relationship

At runtime, the authored manifest remains the source of truth while the loader may prefer a valid cooked `.ctex` artifact.

The manifest still matters because changes to fields like `source`, `srgb`, or `version` invalidate older cooked output.
