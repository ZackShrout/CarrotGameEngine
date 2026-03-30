# Carrot Game Engine – Audio Asset JSON Schema

**BunnySoft**
**Schema Reference Document**
**Last Updated: March 2026**

---

## 1. Purpose

This document defines the authored JSON format used for Carrot **audio asset definitions**.

These files are typically stored as:

* `*.audio.json`

They are used to describe how Carrot should interpret an authored audio asset.

Audio asset JSON is part of the **authoring/import layer** of the engine.

It is used to define things such as:

* asset identity
* source audio file
* default bus
* gain
* looping behavior
* spatial behavior
* streaming behavior

It is **not** a runtime audio object format.

For broader system context, see:

* `docs/systems/asset_pipeline.md`
* `docs/systems/audio_engine.md`
* `docs/systems/json_spec.md`

---

## 2. Design Intent

Audio asset JSON exists to define the **default playback identity and import intent** of an audio asset.

This allows authored sound behavior to live in content metadata rather than forcing gameplay code to restate the same playback defaults repeatedly.

### Important Boundary

Audio asset JSON should describe:

> what an audio asset **means**

—not how the runtime audio engine stores every playback detail internally.

That distinction matters.

---

## 3. File Naming

Audio asset definition files should use the extension:

```text id="7uxxax"
.audio.json
```

Examples:

* `victory.audio.json`
* `oak_battle_theme.audio.json`
* `jalen_theme.audio.json`

The filename does **not** have to be the canonical asset ID, but it should generally remain clear and human-readable.

---

## 4. Minimal Valid Schema

A minimal valid audio asset definition looks like this:

```json id="0of6vr"
{
  "id": "music.victory",
  "source": "engine://audio/Victory!.wav"
}
```

This is sufficient to define an audio asset as long as required fields are valid and all omitted fields can fall back to defaults.

---

## 5. Recommended Full Example

A fuller example:

```json id="vl1mqq"
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

This is a representative example of a music asset that:

* routes to the music bus
* loops
* streams from disk
* does not spatialize

---

## 6. Field Reference

---

## 7. Required Fields

### `id`

**Type:** `string`
**Required:** Yes

The stable engine-facing asset ID for this audio asset.

Example:

```json id="9v8pkz"
{
  "id": "music.oak_battle_theme"
}
```

### Rules

* should be unique within the project’s audio asset space
* should be stable over time
* should be human-readable
* should follow Carrot asset ID conventions

### Good Examples

* `music.oak_battle_theme`
* `sfx.victory`
* `ui.menu_confirm`

---

### `source`

**Type:** `string`
**Required:** Yes

The source audio file used by this asset.

This should generally be a **VFS path**, not a fragile absolute filesystem path.

Example:

```json id="o0q3pp"
{
  "source": "engine://audio/Victory!.wav"
}
```

### Rules

* should point to a valid audio source file
* should typically use Carrot VFS roots such as `engine://` or `game://`
* should refer to source media, not runtime asset handles

---

## 8. Optional Fields

### `bus`

**Type:** `string`
**Required:** No
**Default:** `"sfx"` *(or engine-defined default bus if different)*

Defines the default audio bus this asset should route to.

Example:

```json id="ylw7g0"
{
  "bus": "music"
}
```

### Typical Values

* `"master"` *(rare as a direct asset target)*
* `"music"`
* `"sfx"`
* `"ui"`
* other engine-defined buses as supported

### Notes

This field exists so authored content can define its natural routing without requiring gameplay code to restate it.

---

### `gain`

**Type:** `number`
**Required:** No
**Default:** `1.0`

Defines the default playback gain multiplier for this asset.

Example:

```json id="2sfcrj"
{
  "gain": 0.8
}
```

### Notes

* `1.0` means default unchanged gain
* values above `1.0` are allowed if desired
* values should generally remain sensible and non-negative

This is an authored default, not a hard runtime limitation.

---

### `looping`

**Type:** `boolean`
**Required:** No
**Default:** `false`

Defines whether this asset should loop by default when used as a looping-capable playback asset.

Example:

```json id="3o4je0"
{
  "looping": true
}
```

### Notes

This does not mean every use of the asset must always loop in every context forever, but it defines the authored default behavior.

---

### `loop_start`

**Type:** `integer`
**Required:** No
**Default:** `0`

Defines the loop start position in **sample frames**.

Example:

```json id="bcq7l8"
{
  "loop_start": 3374900
}
```

### Notes

* only meaningful when looping is enabled
* interpreted in sample frames, not seconds
* should be valid relative to the source audio content

---

### `loop_end`

**Type:** `integer`
**Required:** No
**Default:** `0` *(or engine-defined “end of asset” behavior if omitted)*

Defines the loop end position in **sample frames**.

Example:

```json id="z25qba"
{
  "loop_end": 6666100
}
```

### Notes

* only meaningful when looping is enabled
* interpreted in sample frames, not seconds
* should be greater than or equal to `loop_start`
* should not exceed the valid frame range of the source asset

---

### `spatial`

**Type:** `string`
**Required:** No
**Default:** `"none"`

Defines the default spatial behavior of the asset.

Example:

```json id="z2jzh9"
{
  "spatial": "none"
}
```

### Intended Values

Current / expected values include:

* `"none"` — no spatialization
* future engine-supported spatial modes as they are introduced

### Notes

This is an authored default indicating whether the asset should behave like:

* UI/music/non-spatial audio
* or world-positioned audio

Exact runtime behavior is determined by the audio engine.

---

### `streamed`

**Type:** `boolean`
**Required:** No
**Default:** `false`

Defines whether this asset should default to streamed playback rather than full memory-resident sample loading.

Example:

```json id="4k5nhg"
{
  "streamed": true
}
```

### Typical Use Cases

Use `streamed: true` for assets such as:

* music
* long ambience
* other long-form audio

Use `streamed: false` for assets such as:

* sound effects
* UI sounds
* short repeated assets

### Notes

This is an authored playback/default loading policy field, not a runtime voice state field.

---

## 9. Defaults Summary

If omitted, fields should generally resolve as follows:

| Field        | Default                           |
| ------------ | --------------------------------- |
| `bus`        | `"sfx"` *(or engine default)*     |
| `gain`       | `1.0`                             |
| `looping`    | `false`                           |
| `loop_start` | `0`                               |
| `loop_end`   | `0` / engine-defined end behavior |
| `spatial`    | `"none"`                          |
| `streamed`   | `false`                           |

Exact default handling should remain consistent with the engine’s importer implementation.

---

## 10. Validation Expectations

Audio asset JSON should be validated during the **import / load boundary**, not discovered late during runtime playback.

### Validation Should Catch Things Like:

* missing required fields
* wrong field types
* invalid bus names
* invalid spatial values
* malformed VFS paths
* loop ranges that do not make sense
* duplicate asset IDs

### Good Failure Behavior

When validation fails, Carrot should ideally report:

* what file failed
* what field failed
* what was expected
* what should be fixed

### Bad Failure Behavior

Bad behavior would be:

* silently accepting malformed fields
* inventing nonsense fallback behavior without warning
* allowing invalid authored content to fail much later in unrelated runtime systems

Validation should be early, explicit, and actionable.

---

## 11. Example Use Cases

### 11.1 Short UI Sound

```json id="1m89j6"
{
  "id": "ui.menu_confirm",
  "source": "game://audio/menu_confirm.wav",
  "bus": "ui",
  "gain": 1.0,
  "spatial": "none",
  "streamed": false
}
```

### 11.2 World Sound Effect

```json id="uobg1v"
{
  "id": "sfx.torch_fire",
  "source": "game://audio/torch_fire.wav",
  "bus": "sfx",
  "gain": 0.8,
  "looping": true,
  "spatial": "planar",
  "streamed": false
}
```

### 11.3 Music Track

```json id="9xv8xm"
{
  "id": "music.world_map",
  "source": "game://audio/world_map.wav",
  "bus": "music",
  "gain": 1.0,
  "looping": true,
  "loop_start": 102400,
  "loop_end": 4505600,
  "spatial": "none",
  "streamed": true
}
```

---

## 12. Future Compatibility Notes

This schema is expected to evolve over time as Carrot’s audio system grows.

Possible future additions may include fields for:

* distance behavior
* attenuation settings
* pitch defaults
* variation/randomization
* tags / grouping metadata
* future audio authoring extensions

### Important Rule

New fields should be added intentionally and remain consistent with the schema’s purpose:

> define authored audio asset identity and default playback intent

—not replicate the full runtime internals of the audio engine.

---

## 13. Relationship to Runtime Audio

This schema defines the **authoring format** for audio assets.

It does **not** define:

* voice runtime state
* DSP state
* command queue state
* backend audio device behavior
* mixer internals

Those belong to the runtime audio engine.

For runtime architecture, see:

* `docs/systems/audio_engine.md`

---

## 14. Summary

Carrot audio asset JSON exists to define:

* what an audio asset is
* what source it uses
* how it should behave by default
* how the engine should interpret it during import/load

That is its role.

It should remain:

* readable
* stable
* explicit
* tool-friendly
* clearly separated from runtime audio engine internals

## See Also

- `docs/systems/asset_pipeline.md`
- `docs/systems/audio_engine.md`
- `docs/systems/json_spec.md`
