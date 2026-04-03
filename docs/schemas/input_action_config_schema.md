# Carrot Game Engine - Input Action Config JSON

**Status:** Current working schema
**Applies to:** `input_actions.json`
**Last Updated:** April 2026

---

## Purpose

Input action config files define default keyboard bindings for gameplay-facing actions.

This first pass is intentionally small.
It exists to move default bindings out of hardcoded game setup and into authored data while preserving safe code-side fallback behavior.

Current runtime use:

* sandbox default keyboard bindings
* multiple bindings per action
* modifier-aware bindings such as `Alt+Enter`

This format does **not** yet cover:

* user rebinding persistence
* gamepad bindings
* per-platform override layering
* UI-facing control metadata

---

## File Location

Current sandbox default path:

```text
game://config/input_actions.json
```

Current authored file:

[`src/Game/assets/config/input_actions.json`](/Users/zshrout/dev/CarrotGameEngine/src/Game/assets/config/input_actions.json)

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
    { "action": "toggle_fullscreen", "key": "Enter", "mods": ["Alt"] }
  ]
}
```

---

## Binding Entries

Each entry in `bindings` must be an object with:

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

## Current Sandbox Example

```json
{
  "bindings": [
    { "action": "move_up", "key": "W" },
    { "action": "move_up", "key": "Up" },
    { "action": "move_down", "key": "S" },
    { "action": "move_down", "key": "Down" },
    { "action": "move_left", "key": "A" },
    { "action": "move_left", "key": "Left" },
    { "action": "move_right", "key": "D" },
    { "action": "move_right", "key": "Right" },
    { "action": "interact", "key": "E" },
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
* `key` is missing or unknown
* modifier data is malformed

The current goal is to fail clearly and safely rather than partially applying a broken config.
