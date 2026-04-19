# Gameplay Runtime API Examples

**BunnySoft**
**Working usage guide**
**Last Updated: April 18, 2026**

---

## 1. Purpose

This document shows the intended shape of Carrot's cleaned gameplay-facing runtime APIs.

It exists so future games do not need to infer the practical engine contract by reading `Sandbox` source.

The examples below focus on the stable seam between game code and engine runtime systems:

* scene runtime participation
* game-facing camera/view control
* gameplay input routing

---

## 2. Scene Runtime Participation

If a game uses the same player controller, interaction controller, listener, and scene validation callback for most scene loads, set those once on `scene_runtime_t` and let per-request load options only carry what is truly request-specific.

Example:

```cpp
carrot::scene::scene_runtime_t scene_runtime;

scene_runtime.set_default_runtime_bindings({
    .player_controller = &_player_controller,
    .interaction_controller = &_interaction_controller,
    .validate_loaded_scene = validate_loaded_scene,
    .listener = this
});

scene_runtime.request_load(game, "scene.example.town");
scene_runtime.request_load(
    game,
    "scene.example.house",
    carrot::scene::make_scene_load_options({}, "DoorSpawn"));
```

Use this pattern when:

* the same runtime participation applies across normal scene flow
* only the spawn marker or presentation override changes per request

Do not move game-specific meaning into the engine layer just because the binding seam is reusable.
Things such as authored object interpretation, trigger responses, runtime state carryover, and scene-local rules still belong to game code.

---

## 3. Per-Request Overrides

Default runtime bindings are a convenience, not a lock-in.
If a particular scene request needs different participation, explicit load options override the defaults for that request.

Example:

```cpp
scene_runtime.request_load(
    game,
    "scene.example.debug_room",
    carrot::scene::scene_load_options_t{
        .player_controller = &temporary_controller,
        .interaction_controller = &temporary_interaction,
        .listener = &temporary_listener
    });
```

That should be used sparingly.
The common path should stay readable and stable through the default binding surface.

---

## 4. Camera and View Control

Game code should talk to `game_view_t` through explicit camera terminology rather than renderer-flavored helpers.

Example:

```cpp
game.view.set_camera_zoom(2.0f);
game.view.center_camera_on_world_position(game.world, { 24.0f, 12.0f });

const carrot::core::game_view_camera_t snapshot{
    game.view.camera_state(game.world)
};
```

Preferred camera-facing API:

* `camera_zoom()`
* `set_camera_zoom(...)`
* `camera_center_world_position(...)`
* `center_camera_on_world_position(...)`
* `camera_state(...)`
* `set_camera_state(...)`

The engine should keep renderer-stage details behind `game_view_t` so game code stays on a view-level contract.

---

## 5. Input Routing

Games should use engine-owned routing helpers instead of hand-building the default config shape from scratch.

Example:

```cpp
game.input.configure_routing(
    carrot::input::make_fixed_local_multiplayer_routing_config(2u));

LOG_CORE_INFO("Input routing: {}", game.input.describe_routing());
```

Important routing rules:

* `single_player_auto` should remain the simple default path
* `local_multiplayer_fixed` is opt-in
* diagnostics should use `describe_routing()` and per-player description helpers rather than local string formatting

---

## 6. Ownership Line

The cleaned gameplay-facing API line should be:

* engine-owned reusable seams for runtime participation, view control, controller helpers, and routing setup
* game-owned meaning for authored semantics, trigger outcomes, gameplay rules, continuity data, and content-specific policy

If a future game can use Carrot correctly without copying `Sandbox` glue, this layer is doing its job.
