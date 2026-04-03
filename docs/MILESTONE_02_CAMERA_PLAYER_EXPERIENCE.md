# Carrot Game Engine - Milestone 02

**Title:** Camera and Player Experience
**Status:** Proposed
**Focus:** Turn the current playable scene slice into a more intentional gameplay experience by giving camera behavior, input ownership, diagnostics, and scene-state flow a stronger foundation.

---

## Milestone Goal

This milestone builds on the completed scene-flow foundation.

Carrot now has:

* authored scene assets
* scene loading
* scene transitions
* automated scene-path tests
* gameplay-facing input actions

The next structural step is to make the player-facing experience feel more deliberate.

Right now, the biggest remaining “temporary bridge” areas are:

* camera zoom/framing ownership
* config-backed input bindings
* scene/content validation feedback
* multi-scene authoring confidence
* transition-time gameplay state handoff

This milestone focuses on those gaps.

---

## Ticket 1 - Camera Ownership and Default Scene Camera Setup

**Priority:** P0
**Outcome:** Camera zoom and framing are owned by a real camera model rather than temporary scene presentation-scale behavior.

### Why

The current scene path works, but some view behavior is still being expressed through temporary scene presentation defaults.

That was a good bridge for restoring the existing sandbox feel, but it should not become the long-term ownership model.

Carrot already has enough scene flow and world rendering structure that camera policy now deserves a proper home.

### Scope

Introduce a clearer camera ownership model for the playable world flow.

Suggested first responsibilities:

* world camera zoom
* camera follow target selection
* follow smoothing policy if desired
* optional dead-zone or follow-window support
* scene-authored default camera settings

Suggested first authored scene additions:

* default camera zoom
* optional follow mode
* optional initial camera target policy

### Likely Touch Points

* [Game.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Game/Game.cpp)
* [GameView.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Core/GameView.h)
* [Camera2D.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Renderer/Camera/Camera2D.h)
* [SceneLoader.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/World/SceneLoader.cpp)
* [SceneAsset.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Assets/Scene/SceneAsset.h)
* [scene_asset_json_schema.md](/Users/zshrout/dev/CarrotGameEngine/docs/schemas/scene_asset_json_schema.md)

### Implementation Notes

Keep the ownership boundary clear:

* asset `pixels_per_unit` should continue to mean world size
* camera zoom should control how large the world appears on screen
* scene assets should provide camera defaults, not redefine world scale

Avoid pushing camera policy back into renderer-global presentation knobs if the desired behavior is really gameplay-facing view control.

### Acceptance Criteria

* The sandbox no longer depends on temporary scene presentation-scale behavior for its default zoom.
* A scene can provide authored default camera settings.
* The camera still follows the player cleanly after normal scene load and after door transitions.
* Existing sprite/world size semantics remain intact.

---

## Ticket 2 - Config-Backed Input Action Bindings

**Priority:** P1
**Outcome:** Input actions are loaded from authored or config-backed bindings instead of being defined only in code.

### Why

The action layer now exists, which is the hard architectural part.

The next step is to move bindings out of hardcoded setup so Carrot can support:

* per-game defaults
* future user rebinding
* multiple control schemes
* cleaner game-side customization

### Scope

Add a first config-backed binding source for action maps.

Suggested first capabilities:

* load default action bindings from a JSON config file
* support multiple keys per action where useful
* keep modifier-aware bindings such as `Alt+Enter`
* preserve code-side fallback defaults if config is missing

### Likely Touch Points

* [ActionMap.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Input/ActionMap.h)
* [ActionMap.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Input/ActionMap.cpp)
* [Game.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Game/Game.cpp)
* possible new input config asset or runtime config file under `src/Game/assets/`
* test coverage under [/Users/zshrout/dev/CarrotGameEngine/tests/](/Users/zshrout/dev/CarrotGameEngine/tests/)

### Implementation Notes

Keep this first pass focused on defaults and engine boundaries.

This does not need:

* a UI rebinding screen
* save-file persistence yet
* gamepad support unless it falls out naturally

It does need:

* stable action names
* clear parsing/validation
* a shape that later user-rebinding can build on

### Acceptance Criteria

* Sandbox action bindings can be defined in config/authored data rather than only in code.
* Missing config falls back safely to sensible defaults.
* Existing movement, interaction, quit, and fullscreen behavior still works.
* The config path has at least basic automated test coverage.

---

## Ticket 3 - Scene Validation and Authoring Diagnostics

**Priority:** P1
**Outcome:** Scene and transition authoring errors fail early and explain themselves clearly.

### Why

The current scene system works, but as authored content grows, silent failure or weak diagnostics will become one of the biggest productivity drains.

The most valuable next layer is not more runtime magic. It is better authoring feedback.

### Scope

Add stronger validation and logging around scene and transition setup.

Suggested first validations:

* missing tilemap asset ids
* missing player sprite ids
* missing spawn markers
* `Door` targets that reference unknown scene ids
* legacy `target_map` values that do not resolve to a known scene
* malformed input-binding config

### Likely Touch Points

* [SceneAssetManifestImporter.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Assets/Scene/SceneAssetManifestImporter.cpp)
* [SceneLoader.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/World/SceneLoader.cpp)
* [WorldInteractionHelpers.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Game/WorldInteractionHelpers.cpp)
* [ActionMap.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Input/ActionMap.cpp)
* [Logger.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Core/Logger.h)
* test coverage under [/Users/zshrout/dev/CarrotGameEngine/tests/](/Users/zshrout/dev/CarrotGameEngine/tests/)

### Implementation Notes

Prefer diagnostics that answer:

* what failed
* which asset or scene caused it
* what authored field is likely wrong
* whether the engine can recover safely

The goal is to make authored-content mistakes cheap to diagnose.

### Acceptance Criteria

* Invalid scene or door references log clear messages.
* Missing spawn markers fail safely and explain which scene/marker was expected.
* Input-binding config parse/validation failures log clearly and fall back safely where possible.
* At least a few negative-path tests cover the most common failure cases.

---

## Ticket 4 - Multi-Scene Content Workflow and Authoring Pressure Test

**Priority:** P2
**Outcome:** The scene system is verified against a more realistic authored game flow than a single-scene loop.

### Why

A system that works for one map can still hide assumptions that break the moment content becomes slightly more real.

The best way to pressure-test the new architecture is to actually author a second scene and use it as part of normal gameplay flow.

### Scope

Add at least one second authored playable scene and connect it to the existing overworld through real transition data.

Suggested first goals:

* a second tilemap and scene asset
* at least one two-way transition path
* distinct spawn markers on each side
* optionally distinct music or camera defaults per scene

### Likely Touch Points

* new assets under [/Users/zshrout/dev/CarrotGameEngine/src/Game/assets/scenes/](/Users/zshrout/dev/CarrotGameEngine/src/Game/assets/scenes/)
* new tilemap content under [/Users/zshrout/dev/CarrotGameEngine/src/Game/assets/tilemaps/](/Users/zshrout/dev/CarrotGameEngine/src/Game/assets/tilemaps/)
* existing scene and interaction code paths
* docs if any new authoring conventions are introduced

### Implementation Notes

This ticket is intentionally part content, part architecture verification.

The point is not just to add “more map.”
The point is to expose assumptions in:

* scene ids
* spawn routing
* music refresh
* camera defaults
* authored transition conventions

### Acceptance Criteria

* The sandbox includes at least two authored scenes.
* The player can move between them through real authored transitions.
* Scene-local defaults such as music and camera setup apply correctly after transition.
* The system works the same under at least the normal supported local backend paths already used for sandbox testing.

---

## Ticket 5 - Transition-Time Gameplay State Handoff

**Priority:** P2
**Outcome:** Scene transitions preserve a small but intentional set of gameplay state instead of acting like a total reset.

### Why

Right now transitions feel good technically, but they are still mostly “reload the world and continue.”

That is enough for structural proof, but the next step toward a real game foundation is deciding what should survive a transition.

### Scope

Add a minimal transition-state handoff model.

Suggested first preserved state:

* player facing direction
* last-used action context if useful
* simple world flags such as opened chest state or one-shot interaction flags

Keep this intentionally small.

### Likely Touch Points

* [Game.h](/Users/zshrout/dev/CarrotGameEngine/src/Game/Game.h)
* [Game.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Game/Game.cpp)
* [SandboxInteractionController.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Game/SandboxInteractionController.cpp)
* world object or interaction helper code as needed
* possible future save/state helper structures

### Implementation Notes

Do not jump straight to a full save system.

The goal is to identify a minimal runtime handoff structure that:

* survives one scene transition
* is easy to reason about
* can later inform persistence design

### Acceptance Criteria

* At least one meaningful piece of player state survives a scene transition.
* At least one simple world interaction state can survive reload where appropriate.
* The state handoff stays explicit rather than hidden in scattered globals.
* Existing transitions remain smooth and safe.

---

## Recommended Order

Start with `1 -> 3 -> 2 -> 4 -> 5`.

That order gives you:

* camera ownership clarified before more scene behavior piles on
* diagnostics early so new authoring mistakes are cheaper to debug
* config-backed input once the core control model is stable
* a real multi-scene pressure test after those foundations improve
* gameplay-state handoff last, once the transition path itself is more mature

---

## Why This Order

Ticket 1 fixes the biggest remaining architecture ambiguity from the last milestone.

Ticket 3 comes early because content-facing diagnostics pay off immediately while the scene and input systems are still being extended.

Ticket 2 then builds on the now-stable action boundary without competing with camera cleanup.

Ticket 4 is where the milestone starts proving itself under a more real gameplay workflow.

Ticket 5 is best saved for the end, because preserved transition state makes the most sense once scene loading, camera defaults, diagnostics, and multi-scene authoring are already behaving predictably.

---

## Milestone Outcome

When this milestone is done, Carrot should feel less like “the first playable vertical slice” and more like “a small but intentional game engine loop.”

The main signs of success will be:

* camera behavior has a clear architectural owner
* input bindings are no longer code-only
* authored errors are much easier to diagnose
* scene flow has been pressure-tested with more than one map
* transitions preserve the beginnings of real gameplay continuity
