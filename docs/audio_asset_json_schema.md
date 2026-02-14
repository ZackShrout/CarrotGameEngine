# Audio Asset JSON Schema

This document defines the **authoring-time JSON schema** for audio assets. These files describe *intent* and are consumed by the audio asset importer at startup (or offline in the future).

This schema is **human-facing**, **editor-friendly**, and **not used on runtime hot paths**.

---

## File Overview

* File extension: `.audio.json` (recommended, not required)
* Parsed at startup by the engine-owned JSON importer
* Converted into immutable runtime `sound_asset_t` objects

---

## Required Fields

### `id` (string, required)

The globally unique authoring asset ID.

```json
"id": "audio.music.victory"
```

Rules:

* Must be unique across all audio assets
* UTF-8 string
* Hierarchical naming strongly recommended
* Hashed to a `uint64_t asset_id` during import

---

### `sample` (string, required)

Path to the audio sample file relative to the asset root.

```json
"sample": "Audio/Victory.wav"
```

Rules:

* Must resolve to a valid audio file
* Decoded or loaded during import
* Importer is responsible for format validation

---

## Optional Fields (With Defaults)

### `bus` (string, optional)

Target audio bus for playback.

```json
"bus": "music"
```

Default:

```text
"sfx"
```

Importer maps this string to `audio_bus_id`.

---

### `gain` (number, optional)

Base linear gain applied to the sound.

```json
"gain": 1.0
```

Default:

```text
1.0
```

---

### `gain_variance` (number, optional)

Randomized gain variance applied per playback.

```json
"gain_variance": 0.05
```

Default:

```text
0.0
```

Meaning:

* Actual gain = `gain * random(1 ± gain_variance)`

---

### `pitch` (number, optional)

Base pitch multiplier.

```json
"pitch": 1.0
```

Default:

```text
1.0
```

---

### `pitch_variance` (number, optional)

Randomized pitch variance applied per playback.

```json
"pitch_variance": 0.1
```

Default:

```text
0.0
```

---

### `spatial` (string, optional)

Defines how the sound is spatialized.

```json
"spatial": "none"
```

Valid values:

* `"none"`      — no spatialization
* `"planar"`    — 2D planar panning
* `"full_3d"`   — full 3D spatialization

Default:

```text
"none"
```

---

## Looping

### `loop` (object, optional)

Defines seamless looping behavior.

```json
"loop": {
  "start": 0,
  "end": 0
}
```

Fields:

* `start` (integer): loop start frame
* `end`   (integer): loop end frame

Rules:

* `start == end == 0` disables looping
* Frames are sample-frame indices
* Importer validates bounds

Default:

```json
{ "start": 0, "end": 0 }
```

---

## Distance Attenuation (Optional)

### `distance` (object, optional)

Defines distance-based attenuation.

```json
"distance": {
  "model": "linear",
  "min": 1.0,
  "max": 25.0
}
```

Fields:

* `model` (string): `none`, `linear`, `inverse`
* `min`   (number): minimum distance
* `max`   (number): maximum distance

Defaults:

```json
{
  "model": "none",
  "min": 1.0,
  "max": 1.0
}
```

---

## Full Example

```json
{
  "id": "audio.music.victory",
  "sample": "Audio/Victory.wav",

  "bus": "music",

  "gain": 1.0,
  "gain_variance": 0.0,

  "pitch": 1.0,
  "pitch_variance": 0.0,

  "spatial": "none",

  "loop": {
    "start": 0,
    "end": 0
  }
}
```

---

## Importer Responsibilities Summary

The audio asset importer must:

* Validate schema correctness
* Apply defaults for missing fields
* Resolve sample file paths
* Convert string enums to engine enums
* Hash asset ID and detect collisions
* Populate immutable `sound_asset_t`
* Register asset with the audio asset registry

---

## Notes

* This schema is intentionally verbose and explicit
* Authoring convenience is favored over compactness
* Runtime data is derived, not mirrored

This schema may evolve, but backward compatibility should be preserved when possible.
