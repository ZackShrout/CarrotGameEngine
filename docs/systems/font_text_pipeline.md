# Carrot Game Engine – Font and Text Pipeline

**BunnySoft**
**System Design Document**
**Last Updated: April 15, 2026**

---

## 1. Purpose

This document defines the current architecture and near-term direction for Carrot's native runtime text pipeline.

Its immediate purpose is to describe:

* the intended role of a cooked font artifact such as `.cfont`
* the native MSDF text rendering direction
* the import pipeline from source font data into runtime-facing text assets
* the runtime responsibilities for layout, measurement, and rendering

This is no longer a pre-implementation proposal.
Carrot now has a working native MSDF font pipeline including cooked `.cfont` artifacts, runtime font loading, text measurement/layout, and renderer integration for UI and debug text.

The details here should be treated as the current implemented model, with room for iterative revision as the engine grows.

---

## 2. Core Direction

Carrot's runtime text direction is:

* native and engine-owned
* MSDF-based for scalable runtime text rendering
* built around cooked font artifacts rather than runtime parsing of authoring formats
* portable across Vulkan, Metal, and DirectX 12 through a vertex/fragment shader baseline

Important constraint:

* geometry shaders may be explored later as optional experiments
* geometry shaders must **not** be required for the core Carrot text architecture

## 2.1 Implemented Today

Current implemented pieces include:

* authored `*.font.json` font asset manifests
* cooked `.cfont` artifacts
* MSDF atlas generation during import/cook
* runtime font loading and validation
* glyph lookup and kerning access
* `TextLayout` measurement and placement
* renderer text-quad submission paths across the active RHI backends
* UI label rendering
* engine debug/log text rendering

---

## 3. Goals

The text pipeline should provide:

* crisp runtime text at multiple practical UI sizes
* deterministic text measurement for UI layout
* explicit asset ownership and versioning
* clean integration with Carrot's asset pipeline and cooked-asset direction
* no dependence on a third-party runtime text framework

It should be good enough for:

* menus
* dialogue boxes
* settings screens
* HUD text
* debug/runtime overlays

---

## 4. Non-Goals

The first pass should not try to solve everything.

Out of scope for the first pass:

* rich text / markup
* editable text input
* font fallback stacks across many language systems
* advanced shaping for complex scripts
* a universal editable font project format
* geometry-shader-dependent rendering

These may matter later, but they should not block the first native runtime text milestone.

---

## 5. Source-to-Runtime Model

Carrot text should follow the same broad layered model as the rest of the asset system:

1. source font file
2. authored asset definition
3. cooked/imported font artifact
4. runtime loaded font asset

### 5.1 Source Font File

Examples:

* `.ttf`
* `.otf`

This is the original authored input.

### 5.2 Authored Font Asset Definition

Carrot uses an authored asset definition file to describe a runtime font asset.

Current naming:

* `*.font.json`

The authored definition should describe things such as:

* asset ID
* source font URI
* import character set/range policy
* atlas sizing policy
* MSDF generation settings
* default line-height or presentation-related defaults where appropriate

### 5.3 Cooked Font Artifact

The cooked artifact is:

* `.cfont`

This is machine-generated and engine-owned.

It should contain the runtime-facing data needed to measure and render text efficiently.

### 5.4 Runtime Loaded Font Asset

At runtime, Carrot should load `.cfont` into an engine-facing font asset object containing:

* atlas texture resource
* glyph lookup data
* global metrics
* kerning/pair-adjustment data if supported

Gameplay and UI code should work through the loaded font asset and text-layout APIs, not through raw files.

---

## 6. First-Draft `.cfont` Binary Specification

The current `.cfont` format is intentionally narrow, deterministic, and runtime-oriented.

First-pass assumptions:

* little-endian binary format
* self-contained artifact with atlas payload stored inline
* table-oriented layout with fixed-size records where practical
* versioned header so incompatible format changes fail cleanly

### 6.1 File Layout Overview

Current file layout:

1. header
2. invalidation metadata block
3. global metrics block
4. glyph table
5. optional kerning table
6. atlas payload

All offsets should be absolute byte offsets from the start of the file.

### 6.2 Header

Current header struct:

```cpp
struct cfont_header_v1_t
{
    char     magic[8];                 // "CFONT\0\0"
    uint32_t cooked_format_version;    // starts at 1
    uint32_t importer_version;         // importer/generator revision
    uint32_t flags;                    // reserved for future use

    uint32_t invalidation_offset;
    uint32_t invalidation_size_bytes;

    uint32_t global_metrics_offset;
    uint32_t global_metrics_size_bytes;

    uint32_t glyph_table_offset;
    uint32_t glyph_count;

    uint32_t kerning_table_offset;
    uint32_t kerning_pair_count;

    uint32_t atlas_payload_offset;
    uint32_t atlas_payload_size_bytes;

    uint32_t reserved0;
    uint32_t reserved1;
};
```

First-pass rules:

* `magic` must identify the file as a Carrot cooked font artifact
* `cooked_format_version` gates binary compatibility
* `importer_version` allows invalidation from importer changes
* unknown future flags should cause cautious rejection unless explicitly supported

### 6.3 Invalidation Metadata Block

Current invalidation block:

```cpp
struct cfont_invalidation_v1_t
{
    uint64_t source_font_content_hash;
    uint64_t asset_definition_content_hash;
    uint64_t import_settings_hash;
    uint64_t reserved_hash;
};
```

First-pass purpose:

* detect stale artifacts cleanly
* keep validation logic deterministic

This block is not part of gameplay/runtime presentation logic.

### 6.4 Global Metrics Block

Current global metrics struct:

```cpp
struct cfont_global_metrics_v1_t
{
    float em_size;
    float line_height;
    float ascent;
    float descent;
    float underline_position;
    float underline_thickness;

    uint32_t atlas_width;
    uint32_t atlas_height;
    uint32_t atlas_pixel_format;   // engine enum value or cfont-local enum
    uint32_t atlas_channel_layout; // reserved/simple enum for first pass

    float msdf_pixel_range;
    float distance_normalization;
    float reserved_float0;
    float reserved_float1;
};
```

Notes:

* `em_size` helps preserve importer/runtime scale assumptions explicitly
* underline fields are optional for first runtime use, but cheap to preserve if available
* atlas metadata lives here so the runtime can configure the texture/shader path directly

### 6.5 Glyph Table

Current glyph record:

```cpp
struct cfont_glyph_record_v1_t
{
    uint32_t codepoint;
    uint32_t glyph_index;

    float advance;

    float plane_left;
    float plane_top;
    float plane_right;
    float plane_bottom;

    float uv_left;
    float uv_top;
    float uv_right;
    float uv_bottom;
};
```

Field intent:

* `codepoint` is the runtime lookup key
* `glyph_index` preserves a stable importer-side glyph identity where useful
* `advance` is the pen/cursor advance after drawing the glyph
* `plane_*` defines the glyph placement rectangle in layout/render space
* `uv_*` defines atlas sampling bounds

First-pass recommendation:

* sort glyph records ascending by codepoint
* runtime lookup may use binary search first
* later versions can add acceleration tables if needed

### 6.6 Kerning Table

If first-pass kerning is included, use a compact fixed-size record:

```cpp
struct cfont_kerning_pair_v1_t
{
    uint32_t left_codepoint;
    uint32_t right_codepoint;
    float adjustment;
};
```

First-pass rules:

* the kerning table may be absent by setting count and offset to zero
* if present, it should be sorted by `(left_codepoint, right_codepoint)`

### 6.7 Atlas Payload

First-pass recommendation:

* store the atlas payload inline inside `.cfont`

Benefits:

* simpler load path
* self-contained runtime asset
* easier cache invalidation and portability

Recommended first-pass payload expectation:

* one atlas image
* tightly packed byte payload
* enough metadata in the global metrics block to create the runtime texture correctly

### 6.8 Alignment and Padding Rules

Recommended first-pass rules:

* header starts at byte `0`
* all blocks begin on at least `16-byte` boundaries
* fixed-size tables use tightly packed records with no per-record padding beyond normal struct packing defined by the file spec
* reserved fields should be written as zero

### 6.9 Serialization Rules

First-pass serialization rules:

* write little-endian values explicitly
* avoid compiler-ABI-dependent raw struct dumps unless serialization code guarantees exact field order and packing
* validate all offsets/counts before reading any table
* reject malformed files rather than attempting partial recovery in the runtime loader

---

## 7. First-Pass Import Pipeline

The intended first-pass import flow is:

1. read authored `*.font.json`
2. resolve source `.ttf` / `.otf`
3. parse outline and glyph data
4. generate MSDF atlas and glyph metrics
5. write `.cfont`
6. runtime loader reads `.cfont`

### 7.1 Authoring Input

The authored font definition should choose:

* which font source file to import
* which glyph range/set to include
* generation settings that affect atlas and quality

This keeps the import policy explicit and source-controlled.

Recommended first-pass authored fields:

* `id`
* `source`
* `charset`
* `atlas_width`
* `atlas_height`
* `msdf_pixel_range`
* optional `include_kerning`
* optional default presentation values kept clearly separate from raw import settings

### 7.2 Native Import / Generation

Carrot should own the import/generation path.

This includes:

* reading the source font data
* extracting glyph outlines/metrics
* generating MSDF data
* packing the atlas
* writing the cooked artifact

This is real engine work and should be treated as such, not as a trivial helper utility.

### 7.3 Load-Or-Regenerate Behavior

Runtime or tool-facing import flow should behave like:

* if valid `.cfont` exists, load it
* if `.cfont` is missing or stale, regenerate it
* if regeneration fails, surface a clear error

This should align with the broader imported/cooked asset direction in Carrot.

---

## 8. Runtime Font Asset Responsibilities

The loaded runtime font asset should provide:

* glyph lookup by codepoint
* access to global metrics
* access to kerning/pair adjustments where supported
* atlas texture handle/resource
* any shader-facing normalization data required for MSDF rendering

The runtime font asset should **not**:

* parse source font files during normal runtime use
* regenerate atlas data in hot paths
* own UI layout policy beyond exposing the necessary metrics

---

## 9. Text Layout Layer

The font asset alone is not enough.
Carrot also needs an engine-owned text layout layer above it.

The text layout layer should handle:

* measurement of strings/runs
* line breaking and wrapping
* alignment
* cursor-like layout progression for text quads/glyph runs

The text layout layer should produce a runtime-facing result such as:

* measured bounds
* per-line metrics
* positioned glyph quads or glyph run data

This layer belongs above the raw font asset but below high-level UI widgets.

---

## 10. Renderer Integration

The renderer-side text path should:

* consume positioned glyph run data
* submit quads against the atlas texture
* use an MSDF-aware fragment shader
* support colorized UI/runtime text

First-pass recommendation:

* keep text rendering inside the existing textured-quad-centric renderer model where practical

This keeps text aligned with Carrot's current renderer architecture instead of inventing a second unrelated 2D path.

---

## 11. Suggested First-Pass `*.font.json` Shape

The authored font asset definition should stay explicit and small.

Recommended first-pass shape:

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

Recommended first-pass rules:

* `charset.preset` should likely start with a small curated set such as `BasicLatin`
* explicit include/exclude arrays give a clean escape hatch without requiring a giant schema in the first version
* `defaults` should remain runtime-facing convenience values, not importer internals

---

## 12. Recommended First Implementation Slice

If implementation started from this draft, the recommended order would be:

1. define `*.font.json` authored shape around `BasicLatin` first
2. lock `.cfont` header and block layout
3. implement native import/generation for a limited glyph range
4. implement runtime `.cfont` loading and validation
5. implement MSDF shader + quad submission path
6. implement text measurement and simple wrapping
7. prove the path through runtime UI widgets
8. expand charset/import options only after the first path is stable

---

## 13. Open Questions

The biggest questions still open in this draft are:

1. Is `BasicLatin` the right first-pass preset, or should first bring-up include a slightly broader range?
2. How much kerning support is required in the first shipped version?
3. Which subset of source-font parsing/generation work is truly realistic for the first implementation slice?
4. Which runtime-facing defaults belong in `*.font.json` versus staying purely inside higher-level UI styling code?
