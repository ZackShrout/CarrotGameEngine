# Carrot Game Engine - Input Action Config JSON

**Status:** Current working schema
**Applies to:** `input_actions.json`
**Last Updated:** April 2026

---

## Purpose

Input action config files define default keyboard and controller bindings for gameplay-facing actions.

This first pass is intentionally small.
It exists to move default bindings out of hardcoded game setup and into authored data while preserving safe code-side fallback behavior.

Current runtime use:

* sandbox default keyboard bindings
* sandbox default gamepad bindings
* multiple bindings per action
* modifier-aware bindings such as `Alt+Enter`
* button and axis-threshold gamepad bindings

This format does **not** yet cover:

* user rebinding persistence
* per-platform override layering
* UI-facing control metadata

---

## File Location

Current sandbox default path:

```text
game://config/input_actions.json
```

Current authored file:

[`src/Sandbox/assets/config/input_actions.json`](/Users/zshrout/dev/CarrotGameEngine/src/Sandbox/assets/config/input_actions.json)

If the file is missing or invalid, the sandbox falls back to built-in defaults.

---

## Required Root Shape

The root object must contain a `bindings` array.

Example:

```json
{
  "bindings": [
    { "action": "move_up", "key": "W" },
    { "action": "move_up", "key": "Up" },
    { "action": "move_up", "gamepad_button": "DPadUp" },
    { "action": "toggle_fullscreen", "key": "Enter", "mods": ["Alt"] }
  ]
}
```

---

## Binding Entries

Each entry in `bindings` must be an object with an `action` plus exactly one device field:

### `action`

Stable action name.

Example:

```json
"move_up"
```

### `key`

Keyboard key name.

Example:

```json
"W"
```

### `mods`

Optional modifier requirement.

May be omitted, a single string, or an array of strings.

Examples:

```json
"Alt"
```

```json
["Ctrl", "Shift"]
```

### `gamepad_button`

Normalized engine-facing gamepad button name.

### `gamepad_axis`

Normalized engine-facing gamepad axis name.

### `direction`

Required for `gamepad_axis` bindings. Supported values are `Negative` and `Positive`.

### `threshold`

Optional for `gamepad_axis` bindings. Must be greater than `0.0` and less than or equal to `1.0`.

---

## Supported Key Names

Current parser support includes readable names such as:

* letters: `W`, `A`, `S`, `D`
* digits: `0` through `9`
* arrows: `Up`, `Down`, `Left`, `Right`
* function keys: `F1` through `F12`
* special keys: `Escape`, `Enter`, `Tab`, `Backspace`, `Space`, `Delete`, `Insert`

The parser is case-insensitive and ignores spaces, underscores, and hyphens in names.

Examples that resolve the same way:

* `Escape`
* `escape`
* `Left Arrow`
* `left_arrow`
* `left-arrow`

---

## Supported Modifier Names

Current supported modifier names:

* `Shift`
* `Ctrl` or `Control`
* `Alt` or `Option`
* `Super`, `Cmd`, or `Win`

The parser is case-insensitive.

---

## Supported Gamepad Button Names

Current supported names:

* `South`, `East`, `West`, `North`
* `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight`
* `LeftShoulder`, `RightShoulder`
* `LeftStick`, `RightStick`
* `Back`, `Start`

Aliases such as `A`, `B`, `X`, `Y`, `LB`, `RB`, `Select`, and `Menu` also resolve.

---

## Supported Gamepad Axis Names

Current supported names:

* `LeftX`
* `LeftY`
* `RightX`
* `RightY`
* `LeftTrigger`
* `RightTrigger`

---

## Current Sandbox Example

```json
{
  "bindings": [
    { "action": "move_up", "key": "W" },
    { "action": "move_up", "key": "Up" },
    { "action": "move_up", "gamepad_button": "DPadUp" },
    { "action": "move_up", "gamepad_axis": "LeftY", "direction": "Negative", "threshold": 0.5 },
    { "action": "move_down", "key": "S" },
    { "action": "move_down", "key": "Down" },
    { "action": "move_down", "gamepad_button": "DPadDown" },
    { "action": "move_down", "gamepad_axis": "LeftY", "direction": "Positive", "threshold": 0.5 },
    { "action": "move_left", "key": "A" },
    { "action": "move_left", "key": "Left" },
    { "action": "move_left", "gamepad_button": "DPadLeft" },
    { "action": "move_left", "gamepad_axis": "LeftX", "direction": "Negative", "threshold": 0.5 },
    { "action": "move_right", "key": "D" },
    { "action": "move_right", "key": "Right" },
    { "action": "move_right", "gamepad_button": "DPadRight" },
    { "action": "move_right", "gamepad_axis": "LeftX", "direction": "Positive", "threshold": 0.5 },
    { "action": "interact", "key": "E" },
    { "action": "interact", "gamepad_button": "South" },
    { "action": "quit", "key": "Escape" },
    { "action": "toggle_fullscreen", "key": "F11" },
    { "action": "toggle_fullscreen", "key": "Enter", "mods": ["Alt"] }
  ]
}
```

---

## Validation Behavior

The config currently fails and falls back if:

* the root is not an object
* `bindings` is missing or not an array
* a binding entry is not an object
* `action` is missing or empty
* the binding does not specify exactly one supported device field
* `key` is unknown
* `gamepad_button` is unknown
* `gamepad_axis` is unknown
* `direction` is missing or invalid for axis bindings
* `threshold` is non-numeric or out of range
* modifier data is malformed

The current goal is to fail clearly and safely rather than partially applying a broken config.
