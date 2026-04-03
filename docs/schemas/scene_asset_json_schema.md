# Carrot Game Engine - Scene Asset JSON

**Status:** Current working schema
**Applies to:** `*.scene.json`
**Last Updated:** April 2026

---

## Purpose

Scene assets define the authored setup needed to bootstrap a playable world scene without hardcoding that setup directly in game bootstrap code.

This format is intentionally small in the first pass. It exists to describe:

* which tilemap scene content should load
* which player sprite should be spawned
* where the player should start
* optional initial music
* scene-level camera defaults
* scene-level bootstrap defaults

It does **not** attempt to be a full editor scene graph or a universal save/load format.

The schema is now live in the sandbox scene-loading path and supports scene reload/transition flow with optional spawn-marker override at runtime.

---

## Required Fields

### `id`

Stable logical scene asset id.

Example:

```json
"id": "scene.test.overworld"
```

### `tilemap`

Logical tilemap asset id used as the main environment source for the scene.

Example:

```json
"tilemap": "tilemap.test.overworld"
```

### `player_sprite`

Logical sprite asset id used to construct the initial player world object.

Example:

```json
"player_sprite": "sprite.vraden"
```

---

## Optional Fields

### `map_object_name`

Name assigned to the runtime world object that owns the scene tilemap draw component.

Default:

```json
"OverworldMap"
```

### `initial_music`

Logical audio asset id to start when the scene boots.

Example:

```json
"initial_music": "music.oak_battle_theme"
```

### `player_spawn_marker`

Name of the imported marker object used to place the player after scene load.

Default:

```json
"PlayerSpawn"
```

### `player_name`

Name assigned to the created player world object.

Default:

```json
"Vraden"
```

### `player_type`

Type string assigned to the created player world object.

Default:

```json
"Character"
```

### `camera`

Optional camera defaults applied when the scene loads.

Defaults:

```json
"camera": {
  "zoom": 4,
  "follow_mode": "player",
  "initial_target": "player",
  "dead_zone_world_size": { "x": 2.0, "y": 1.5 },
  "follow_smoothing": 10.0
}
```

Current supported fields:

* `zoom`
* `follow_mode`
* `initial_target`
* `dead_zone_world_size`
* `follow_smoothing`

Example:

```json
"camera": {
  "zoom": 4,
  "follow_mode": "player",
  "initial_target": "player",
  "dead_zone_world_size": { "x": 2.0, "y": 1.5 },
  "follow_smoothing": 10.0
}
```

Supported `follow_mode` values:

* `"player"`
* `"none"`

Supported `initial_target` values:

* `"player"`
* `"spawn_marker"`

`dead_zone_world_size` is authored in world units. A value of `{ "x": 0, "y": 0 }` disables the dead zone.

`follow_smoothing` controls how quickly the camera moves toward its desired follow position.
A value of `0` disables smoothing and snaps immediately when the follow target leaves the dead zone.

### `presentation_origin_px`

Pixel-space presentation origin offset applied to world rendering.

Example:

```json
"presentation_origin_px": { "x": 0, "y": 0 }
```

### `tilemap_world_position`

World-space position of the runtime tilemap world object.

Example:

```json
"tilemap_world_position": { "x": 0, "y": 0 }
```

---

## Example

```json
{
  "id": "scene.test.overworld",
  "tilemap": "tilemap.test.overworld",
  "player_sprite": "sprite.vraden",
  "map_object_name": "OverworldMap",
  "initial_music": "music.oak_battle_theme",
  "player_spawn_marker": "PlayerSpawn",
  "player_name": "Vraden",
  "player_type": "Character",
  "camera": {
    "zoom": 4,
    "follow_mode": "player",
    "initial_target": "player",
    "dead_zone_world_size": { "x": 2.0, "y": 1.5 },
    "follow_smoothing": 10.0
  },
  "presentation_origin_px": { "x": 0, "y": 0 },
  "tilemap_world_position": { "x": 0, "y": 0 }
}
```

---

## Design Notes

The current schema is deliberately narrow.

Near-term evolution may include:

* scene-local actor declarations
* stronger validation rules

Camera zoom now owns how large the world appears on screen.
Scene assets should not redefine world/unit sizing through temporary render-scale fields.
