# Carrot Game Engine – Font Asset JSON Schema

**BunnySoft**
**Schema Reference Document**
**Last Updated: April 10, 2026**

---

## 1. Purpose

This document defines the proposed authored JSON format used for Carrot **font asset definitions**.

These files are expected to be stored as:

* `*.font.json`

They are used to describe how Carrot should import a source font into a cooked runtime font artifact such as `.cfont`.

Font asset JSON is part of the **authoring/import layer** of the engine.

It is used to define things such as:

* asset identity
* source font file
* glyph-set import scope
* atlas sizing policy
* MSDF generation settings

It is **not** a runtime font object format.

For broader system context, see:

* [asset_pipeline.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/asset_pipeline.md)
* [json_spec.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/json_spec.md)
* [font_text_pipeline.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/font_text_pipeline.md)

---

## 2. Design Intent

Font asset JSON exists to define the **import intent** of a runtime font asset.

This allows Carrot to treat font import as an explicit authored pipeline step rather than a hidden runtime behavior.

### Important Boundary

Font asset JSON should describe:

> what font data Carrot should import and how broad that import should be

not how the runtime text system stores every glyph internally.

That distinction matters.

---

## 3. File Naming

Font asset definition files should use the extension:

```text
.font.json
```

Examples:

* `ui_default.font.json`
* `dialogue.font.json`
* `debug_mono.font.json`

The filename does **not** have to be the canonical asset ID, but it should remain clear and human-readable.

---

## 4. Minimal Valid Schema

A minimal valid font asset definition looks like this:

```json
{
  "version": 1,
  "id": "font.ui.default",
  "source": "game://fonts/MyFont.ttf",
  "charset": {
    "preset": "BasicLatin"
  },
  "msdf": {
    "atlas_width": 1024,
    "atlas_height": 1024,
    "pixel_range": 8
  }
}
```

This is sufficient for a first-pass import as long as required fields are valid and omitted optional fields fall back to documented defaults.

---

## 5. Recommended Full Example

```json
{
  "version": 1,
  "id": "font.ui.default",
  "source": "game://fonts/MyFont.ttf",
  "charset": {
    "preset": "BasicLatin",
    "include_codepoints": [],
    "exclude_codepoints": []
  },
  "msdf": {
    "atlas_width": 1024,
    "atlas_height": 1024,
    "pixel_range": 8,
    "include_kerning": true
  },
  "defaults": {
    "line_height_scale": 1.0
  }
}
```

This is a representative example of a runtime UI font asset that:

* imports a source TTF through the VFS
* limits first-pass import scope to a practical preset
* generates an MSDF atlas
* includes kerning data when available

---

## 6. Required Fields

### `version`

**Type:** `integer`
**Required:** Yes

Schema version for the authored font asset definition.

First-pass valid value:

```json
1
```

### Notes

* first implementation should start at `1`
* unsupported versions should fail import clearly

---

### `id`

**Type:** `string`
**Required:** Yes

The stable engine-facing asset ID for this font asset.

Example:

```json
{
  "id": "font.ui.default"
}
```

### Rules

* should be unique within the project's font asset space
* should be stable over time
* should be human-readable
* should follow Carrot asset ID conventions

### Good Examples

* `font.ui.default`
* `font.ui.dialogue`
* `font.debug.mono`

---

### `source`

**Type:** `string`
**Required:** Yes

The source font file used by this asset.

This should generally be a **VFS path**, not a fragile absolute filesystem path.

Example:

```json
{
  "source": "game://fonts/MyFont.ttf"
}
```

### Rules

* should point to a valid source font file
* should typically use Carrot VFS roots such as `engine://` or `game://`
* should refer to authoring/source media, not a cooked artifact

---

### `charset`

**Type:** `object`
**Required:** Yes

Defines which glyphs Carrot should import into the cooked font artifact.

Minimal example:

```json
{
  "charset": {
    "preset": "BasicLatin"
  }
}
```

#### Required Child Field: `preset`

**Type:** `string`
**Required:** Yes

First-pass supported/recommended value:

* `"BasicLatin"`

The first implementation should intentionally start small and clear.

#### Optional Child Field: `include_codepoints`

**Type:** `array<number>`
**Required:** No
**Default:** `[]`

Explicit codepoints to include in addition to the selected preset.

Example:

```json
{
  "include_codepoints": [169, 8212]
}
```

#### Optional Child Field: `exclude_codepoints`

**Type:** `array<number>`
**Required:** No
**Default:** `[]`

Explicit codepoints to exclude from the selected preset.

Example:

```json
{
  "exclude_codepoints": [96]
}
```

### Notes

* the first shipped path should likely begin with `BasicLatin`
* include/exclude arrays provide a small escape hatch without demanding a huge first-pass charset schema

---

### `msdf`

**Type:** `object`
**Required:** Yes

Defines the first-pass MSDF import/generation settings.

Minimal example:

```json
{
  "msdf": {
    "atlas_width": 1024,
    "atlas_height": 1024,
    "pixel_range": 8
  }
}
```

#### Required Child Field: `atlas_width`

**Type:** `integer`
**Required:** Yes

Target atlas width in pixels.

#### Required Child Field: `atlas_height`

**Type:** `integer`
**Required:** Yes

Target atlas height in pixels.

#### Required Child Field: `pixel_range`

**Type:** `number`
**Required:** Yes

Distance range value used by the MSDF generation path.

First-pass expectation:

* should be positive
* should remain in a sane authored range such as `4` to `16` unless later tuning proves otherwise

#### Optional Child Field: `include_kerning`

**Type:** `boolean`
**Required:** No
**Default:** `true`

Indicates whether kerning data should be imported when available.

Example:

```json
{
  "include_kerning": true
}
```

### Notes

* these are import/generation settings, not styling settings
* runtime styling should live above the font asset layer

---

## 7. Optional Fields

### `defaults`

**Type:** `object`
**Required:** No

Optional runtime-facing convenience defaults associated with the font asset.

These should stay lightweight and should not turn the font asset into a full UI style definition.

#### Optional Child Field: `line_height_scale`

**Type:** `number`
**Required:** No
**Default:** `1.0`

Runtime-facing default line-height multiplier for systems that choose to honor it.

Example:

```json
{
  "defaults": {
    "line_height_scale": 1.0
  }
}
```

### Notes

* this field is optional on purpose
* text layout systems may ignore these defaults if a higher-level UI style system overrides them

---

## 8. Validation Rules

The first-pass importer should reject or warn on the following cases as appropriate:

### Reject

* missing `version`
* unsupported `version`
* missing `id`
* missing `source`
* missing `charset`
* missing `msdf`
* non-positive atlas dimensions
* non-positive `pixel_range`
* invalid VFS path in `source`

### Warn

* very small atlas dimensions likely to overflow practical glyph ranges
* duplicate codepoints inside include/exclude lists
* a codepoint present in both include and exclude lists
* importer settings likely to produce low-quality results

---

## 9. Example

```json
{
  "version": 1,
  "id": "font.ui.default",
  "source": "game://fonts/MyFont.ttf",
  "charset": {
    "preset": "BasicLatin",
    "include_codepoints": [169],
    "exclude_codepoints": []
  },
  "msdf": {
    "atlas_width": 1024,
    "atlas_height": 1024,
    "pixel_range": 8,
    "include_kerning": true
  },
  "defaults": {
    "line_height_scale": 1.0
  }
}
```

---

## 10. Design Notes

The current schema is deliberately narrow.

Near-term evolution may include:

* richer preset ranges beyond `BasicLatin`
* more explicit glyph-range authoring models
* additional import-quality controls
* stronger validation guidance for atlas sizing

The first version should prioritize:

* clarity
* explicitness
* a practical path to `.cfont`

over trying to solve every future text-authoring concern at once.
