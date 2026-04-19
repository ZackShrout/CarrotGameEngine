# Carrot Input Router Direction

**BunnySoft**
**Working system direction**
**Last Updated: April 7, 2026**

---

## 1. Purpose

This document captures the direction for optional local-multiplayer input routing in Carrot.

It defines how Carrot can support multi-player controller assignment without forcing all games to adopt multiplayer input complexity.

---

## 2. Core Principle

Local multiplayer input in Carrot should be **opt-in, policy-driven, and game-owned**.

That means:

* single-player defaults should remain simple and unchanged
* multiplayer routing should activate only when a game explicitly enables it
* engine systems should provide reusable routing primitives, not game-rule assumptions

---

## 3. System Boundaries

Carrot should keep device state, routing, and gameplay intent as separate layers.

### 3.1 Controller Manager (Device Layer)

Responsible for:

* discovering controllers
* tracking per-slot normalized controller state
* exposing active-controller convenience for simple single-player flows

Not responsible for:

* assigning devices to players
* join/leave game rules
* per-player action semantics

### 3.2 Input Router (Policy Layer, Optional)

Responsible for:

* mapping devices to logical player input contexts
* applying an explicit routing policy selected by game code
* handling runtime re-assignment when devices connect/disconnect

Not responsible for:

* gameplay mechanics
* character control logic
* menu design or UX decisions

### 3.3 Player Input Context (Gameplay-Facing Layer)

Responsible for:

* per-player action-map state
* per-player movement intent
* per-player action queries (`is_pressed`, `pressed_this_frame`, etc. as they evolve)

Not responsible for:

* hardware enumeration
* low-level platform controller integration

---

## 4. Opt-In Configuration Model

Default behavior should remain single-player and compatible with current games.

Recommended modes:

* `single_player_auto` (default)
  * existing behavior
  * one primary gameplay context
* `local_multiplayer_fixed`
  * explicit mapping such as P1 -> slot 0, P2 -> slot 1
* `local_multiplayer_join`
  * join flow such as "press start/options to join"

Games should explicitly pick a mode.
If they do not configure multiplayer, Carrot should behave exactly as it does today.

---

## 5. API Direction (High-Level)

Current engine-facing setup should stay simple and explicit:

* keep `active_gamepad()` and current single-player helpers
* keep broader routing opt-in from game code
* expose per-player input context access
* provide engine-owned setup helpers so games do not need to hand-roll the default config shape

Current shape:

```cpp
game.input.configure_routing(carrot::input::make_single_player_routing_config());

game.input.configure_routing(
    carrot::input::make_fixed_local_multiplayer_routing_config(2u));

auto& p1 = game.input.player(0);
auto& p2 = game.input.player(1);
```

Games that never call routing setup should keep single-player semantics only.

Important behavior:

* `single_player_auto` is normalized to one primary gameplay context
* the normalized single-player context receives keyboard input and uses active-gamepad convenience
* `local_multiplayer_fixed` keeps authored assignments and exposes one runtime context per configured player
* `describe_routing()` and per-player description helpers should be used for diagnostics rather than local string formatting

---

## 6. Non-Goals

This direction does not include:

* online/rollback netcode input architecture
* user-facing rebinding UX implementation
* per-platform controller icon and prompt systems
* automatic gameplay behavior for games that did not opt in

---

## 7. Suggested Rollout

1. Expose per-slot controller queries cleanly from existing controller infrastructure.
2. Introduce optional input router scaffolding with fixed assignment mode.
3. Add per-player input contexts backed by existing action-map behavior.
4. Add join-style policy only after fixed assignment is stable.
5. Keep sandbox/example usage optional and explicit rather than silently auto-enabled.

---

## 8. Success Criteria

This direction is successful if:

* current single-player games remain unaffected by default
* a game can explicitly enable two-player local input without raw-device gameplay code
* controller assignment logic is testable and engine-owned
* future multiplayer input work can expand without replacing the single-player path
