# Carrot Game Engine - Milestone 17

**Last Updated:** April 15, 2026
**Title:** Runtime Iteration Expansion and Scene Inspection Tooling
**Status:** Completed and archived
**Focus:** Strengthen Carrot's engine-owned runtime iteration loop and validate it through deeper editor-side inspection of assets, scenes, and live world state without crossing into editor-owned scene authoring.

---

## Milestone Goal

Milestones 13 through 16 gave Carrot a strong base:

* engine-owned imported/cooked assets
* structured runtime iteration diagnostics for an initial asset slice
* a thin optional `CarrotEditor` host
* scene runtime lifecycle and async transition structure
* stronger gameplay-facing world/runtime APIs

That means the next bottleneck is no longer "can Carrot load and run real authored content?"

It can.

The next bottleneck is:

* how quickly authored changes can be validated
* how clearly the engine explains what is currently loaded
* how easily runtime world state can be inspected without digging through logs
* whether the editor remains a thin observer of engine truth instead of becoming a second ownership layer

Milestone 17 exists to make the runtime easier to trust while preserving the architecture Carrot has been converging toward.

This milestone is successful if Carrot ends with:

* stronger automatic and manual runtime iteration behavior
* broader diagnostics across the most important asset/runtime boundaries
* a useful scene/world inspection surface in `CarrotEditor`
* explicit, honest reload/rebuild rules for assets that cannot safely hot reload live
* no editor-owned scene authority

---

## Scope Summary

Milestone 17 is:

* a runtime iteration milestone
* a diagnostics and inspection milestone
* an editor-boundary validation milestone
* a live scene/world visibility milestone

Milestone 17 is not:

* a scene editing milestone
* a map editing milestone
* a prefab/archetype authoring milestone
* a property editing framework milestone
* a full debugging IDE milestone
* a replacement for Tiled or Aseprite

The core rule is:

**The engine owns runtime state.**
**The editor observes, queries, highlights, and triggers safe actions.**
**The editor does not become the source of truth for scene or world data.**

---

## Why This Milestone Comes Next

Carrot now has enough real runtime structure that iteration friction matters more than adding another isolated subsystem.

Current strengths already in place:

* scene runtime and transition lifecycle
* loaded world/object structure
* collision and trigger systems
* authored tilemap/world import
* input routing and rebinding
* first useful asset diagnostics
* a thin editor host already running on top of engine APIs

Current pain points still visible:

* runtime iteration coverage is still narrow
* many runtime truths are still easiest to discover through logs
* the editor can inspect some asset state, but not the active scene/world well enough
* rebuild-required assets and live-reload-safe assets need clearer user-facing separation
* scene/world debugging still leans too heavily on ad hoc sandbox proof

That makes the right next move:

* deepen visibility
* harden iteration
* preserve boundaries
* avoid prematurely turning `CarrotEditor` into a world editor

---

## Core Architectural Rule

Runtime inspection is an **engine capability surfaced through tooling**, not editor-owned knowledge.

The engine should own:

* asset invalidation and reload policy
* scene runtime state
* active world/object/light/trigger/collision truth
* transition state and diagnostics
* reload/rebuild decisions

The editor may:

* query
* display
* filter
* inspect
* preview
* request safe reload/rebuild actions

The editor may not:

* maintain shadow copies of world state
* invent private scene data models
* bypass asset/scene/runtime systems
* become required for correct engine behavior

If a scene/runtime feature only works correctly when the editor is open, it does not belong in this milestone.

---

## Primary Deliverables

### 1. Runtime Iteration Expansion

Carrot should expand iteration support beyond the current first slice.

Required outcomes:

* maintain current manual reload support for safe assets
* add automatic change detection for the safe initial asset slice
* broaden structured diagnostics to cover at least:
  * textures
  * sprites
  * audio
  * tilemaps
  * scenes
  * fonts where practical
* expose reload/rebuild requirement clearly rather than hiding it in logs

Important boundary:

This milestone does not need to claim that every asset reloads live.
It does need to state clearly what happens for each asset class.

### 2. Explicit Reload Safety Model

Carrot should make reload behavior visible and honest per asset type.

Expected policy categories:

* `reloadable_live`
* `reloadable_on_next_use`
* `manual_refresh_only`
* `restart_or_scene_rebuild_required`

Required outcomes:

* editor and runtime diagnostics both surface the policy
* scene-bound assets such as tilemaps/scenes can report when live reload is unsafe
* the engine can expose a safe "rebuild current scene" path where appropriate

Important boundary:

This milestone should prefer "clear and boring" over "magical and vague."

### 3. Scene Runtime Inspection Surface

`CarrotEditor` should gain a first useful scene inspector.

Required minimum surface:

* active scene id
* current runtime state
* transition state/phase/progress
* current spawn/context summary where relevant
* camera summary
* loaded world summary:
  * object count
  * trigger count
  * collider count
  * light count

This should make it easy to answer:

* what scene is actually active
* what phase the runtime is in
* whether a transition is in progress
* whether a rebuild/reload actually took effect

### 4. World Object Inspection Surface

The editor should expose a browsable runtime object list for the active scene/world.

Required first-pass fields:

* object id
* object name
* object type
* transform summary
* source provenance if authored
* key attached components:
  * sprite
  * tilemap
  * collision
  * trigger
  * animator
  * visibility-related data where practical

Selecting an object should show richer details without implying editability.

Important boundary:

This is an inspector, not a property editor.

### 5. Runtime Systems Inspection Surface

The editor should expose first useful summaries for runtime-owned world systems.

Recommended first slice:

* current world lighting summary
  * ambient light
  * active point lights
* collision summary
* layering/visibility summary
* current controlled player object / camera target where applicable

This work matters because many debugging questions are really system-state questions, not asset questions.

### 6. Engine Diagnostics API Growth

The engine should expose public inspection/query APIs sufficient for the editor to consume without backdoors.

Required outcomes:

* editor does not reach through private internals arbitrarily
* inspection data is available from engine-owned runtime-facing seams
* diagnostics are queryable in both sandbox and editor builds
* logs remain useful but are no longer the only truth surface

---

## Required Minimum Slice

To keep the milestone sharp, the minimum acceptable implementation should be:

1. automatic file watching for the safe existing asset slice
2. broader diagnostics/status support for tilemap and scene assets
3. active scene runtime summary in `CarrotEditor`
4. runtime object list and selection/details panel
5. light/trigger/collision summary panel
6. explicit rebuild-required messaging for unsafe live reload cases

If these are solid, the milestone succeeds even if more ambitious inspection polish is deferred.

---

## Suggested Editor Surface

A practical first layout for `CarrotEditor` should be:

### Left Panel: Assets

* asset list/browser
* filter/search later if needed
* selected asset diagnostics
* reload/rebuild action buttons

### Center/Top Panel: Scene Runtime

* active scene
* transition state
* camera summary
* world summary counts
* recent runtime diagnostic messages

### Center/Bottom Panel: Runtime Objects

* scrollable list of current world objects
* selection support
* simple category/filter controls only if cheap

### Right Panel: Selection Details

* selected object details
* selected asset details
* selected light/trigger/collider details depending on current selection mode

This is intentionally enough to be useful without turning into a docking/layout-framework milestone.

---

## Non-Goals

To prevent scope creep, Milestone 17 must not:

* add transform gizmos
* support drag/move/rotate world objects
* persist edits back to source files
* add in-editor light authoring
* add in-editor trigger authoring
* replace Tiled scene/map authoring
* invent a generic property inspector architecture
* become a "full editor" milestone

If editing begins to dominate the design, the milestone has drifted.

---

## Suggested Work Order

1. Expand engine-side asset/runtime diagnostics APIs.
2. Add automatic change detection for the safe asset slice.
3. Expose explicit scene rebuild requirements for scene-bound assets.
4. Add active scene/runtime summary queries.
5. Add runtime world/object/light/trigger/collision inspection queries.
6. Build the new editor panels on top of those public seams.
7. Add targeted tests for reload policy, scene rebuild triggers, and inspection data validity.
8. Close with sandbox/editor validation and doc updates.

This ordering keeps the engine boundary honest and prevents UI work from masking missing runtime architecture.

---

## Ticketed Work Order

This milestone is broad enough that it should be implemented through focused tickets with a clear dependency order.

### Ticket 1. Broaden Runtime Iteration Status and Reload Policy Coverage

Goal:

* expand the engine-owned iteration/diagnostics model beyond the current first asset slice

Expected outcomes:

* runtime iteration status is exposed consistently for:
  * textures
  * sprites
  * audio
  * tilemaps
  * scenes
  * fonts where practical
* per-asset reload policy is surfaced explicitly
* last-attempt result, load origin, cooked artifact state, and invalidation reason remain queryable across the broader slice

Why this comes first:

* automatic watching, rebuild UX, and editor inspection all depend on a trustworthy diagnostics/query model first

### Ticket 2. Automatic Change Detection for Safe Asset Classes

Goal:

* make the current runtime iteration loop more useful without exaggerating what can hot reload safely

Expected outcomes:

* file watching or equivalent change detection for the safe initial slice
* automatic invalidation/reload where live reload is genuinely safe
* clear status updates when automatic reload succeeds or fails
* runtime behavior still works correctly when no editor is open

Why this comes early:

* this is the most direct day-to-day iteration win, but it should only sit on top of the stronger status model from Ticket 1

### Ticket 3. Scene-Bound Rebuild Rules and Rebuild Trigger Path

Goal:

* make scene/tilemap/runtime-structure changes explicit instead of ambiguous

Expected outcomes:

* tilemap and scene diagnostics can report when live reload is unsafe
* rebuild-required cases are distinguishable from safe live reload cases
* the engine exposes a safe "rebuild current scene" path or equivalent runtime action
* rebuild messaging is queryable and visible to tooling

Why this belongs before editor scene inspection:

* the editor should report truthful actionability, not just show state without explaining what the user can do next

### Ticket 4. Scene Runtime Summary and Transition Inspection API

Goal:

* expose the active scene/runtime truth through public engine seams

Expected outcomes:

* active scene id
* runtime state and transition phase/progress
* spawn/context summary where applicable
* camera summary
* aggregate world summary counts for objects, triggers, colliders, and lights

Why this comes here:

* this is the first meaningful step from asset inspection into live runtime inspection, and it should land before object-level detail work

### Ticket 5. Runtime Object Inspection API

Goal:

* make live world contents inspectable in a structured way

Expected outcomes:

* browsable runtime object list for the active world
* stable summary fields per object:
  * id
  * name
  * type
  * transform summary
  * authored source provenance where available
* attached component summaries for the first useful slice:
  * sprite
  * tilemap
  * collision
  * trigger
  * animator
  * visibility-related data where practical

Why this comes after scene summary:

* object-level inspection is much easier to shape well once the higher-level runtime summary contract is already settled

### Ticket 6. Runtime Systems Inspection API

Goal:

* expose the first useful runtime-owned system summaries that explain behavior beyond individual objects

Expected outcomes:

* world lighting summary including ambient and active point lights
* collision summary
* layering/visibility summary
* current controlled player object and/or camera target summary where applicable

Why this is separate from Ticket 5:

* many debugging questions are about world state and system state rather than any one object, so this should not be forced awkwardly into the object inspector contract

### Ticket 7. `CarrotEditor` Scene Inspector Panels

Goal:

* validate the new engine inspection seams through real editor surfaces without crossing into authoring

Expected outcomes:

* scene runtime summary panel
* runtime object list panel
* selection/details panel
* light/trigger/collision summary surface
* explicit reload/rebuild messaging visible alongside asset and scene inspection

Important boundary:

* this ticket must not expand into transform editing, gizmos, or source-authoring workflows

Why this comes late:

* the editor should consume stable engine-facing inspection APIs rather than inventing temporary private data paths

### Ticket 8. Validation, Regression Coverage, and Closeout

Goal:

* harden the milestone and document its actual limitations honestly

Expected outcomes:

* tests for broader iteration status coverage
* tests for rebuild-required vs live-reload-safe policy behavior
* tests for scene/runtime inspection query validity
* sandbox/editor proof that validates the full milestone loop
* doc updates describing the delivered runtime/tooling boundary and current limitations

Why this stays last:

* closeout should protect the final architecture rather than getting invalidated by later ticket churn

---

## Recommended Implementation Order

The recommended implementation order is:

1. Ticket 1. Broaden Runtime Iteration Status and Reload Policy Coverage
2. Ticket 2. Automatic Change Detection for Safe Asset Classes
3. Ticket 3. Scene-Bound Rebuild Rules and Rebuild Trigger Path
4. Ticket 4. Scene Runtime Summary and Transition Inspection API
5. Ticket 5. Runtime Object Inspection API
6. Ticket 6. Runtime Systems Inspection API
7. Ticket 7. `CarrotEditor` Scene Inspector Panels
8. Ticket 8. Validation, Regression Coverage, and Closeout

Why this order fits the milestone:

* Ticket 1 creates the diagnostics truth surface the rest of the milestone depends on.
* Ticket 2 delivers the first immediate workflow win while staying grounded in that truth surface.
* Ticket 3 prevents scene-bound reload behavior from becoming fuzzy or misleading as iteration expands.
* Ticket 4 establishes the high-level runtime inspection contract before deeper object/system detail is added.
* Tickets 5 and 6 then flesh out world inspection in two clean layers: object-centric and system-centric.
* Ticket 7 validates the architecture through the editor only after the engine seams are stable enough to consume directly.
* Ticket 8 is intentionally last so tests, docs, and closeout reflect the real delivered boundary rather than an earlier draft of it.

---

## Success Criteria

Milestone 17 is successful when all of the following are true:

* changing a safe asset produces understandable runtime iteration results
* unsafe live reload cases are reported explicitly and truthfully
* `CarrotEditor` can show the active runtime scene and world state without privileged hacks
* a developer can identify loaded objects, lights, triggers, and collision state without relying only on logs
* the editor can disappear and the runtime still behaves correctly
* no scene editing architecture is required for the milestone to feel useful

---

## Delivered Result

Milestone 17 shipped with the planned engine/runtime inspection seams and a first useful `CarrotEditor` inspection surface.

Delivered outcomes:

* runtime iteration status/policy coverage broadened across texture, sprite, audio, font, tilemap, and scene assets
* automatic change detection for the safe live-reload slice
* explicit scene rebuild path for scene-bound reload cases
* engine-owned scene runtime summary API
* engine-owned runtime object inspection API
* engine-owned runtime systems inspection API
* `CarrotEditor` panels for asset diagnostics, scene summary, systems summary, runtime object list, and selected object details
* validation coverage for the new inspection and rebuild seams

Important implementation note:

* the current editor scene/object/system panels inspect **live runtime state**
* they do not inspect authored scene contents before runtime exists
* the current editor build uses a **temporary hosted runtime harness** that boots `scene.sandbox.town` on startup so the panels can be validated against real data

That harness is intentionally not final editor architecture. It exists to validate the runtime inspection loop and may be removed or replaced later without changing the underlying engine inspection APIs.

---

## Known Limitations

Milestone 17 intentionally stops short of a fuller editor/runtime tooling stack.

Current limitations:

* `CarrotEditor` is still keyboard/gamepad-first; mouse focus/selection behavior is not yet a goal of the engine UI layer
* the current runtime object/details surface is dense and reveals the need for future UI primitives such as scroll/clipping regions
* the current editor-hosted runtime path is a narrow validation harness, not a generalized editor play/runtime architecture
* the scene inspector shows **runtime truth**, not authored pre-runtime scene contents
* continuity/flag-store and trigger-monitor state are not yet surfaced as dedicated scene-runtime system summaries because those stores are not currently owned by `scene_runtime_t`

---

## Validation Summary

Validation completed for the delivered milestone work:

* `CarrotEngineTests` passes with the new scene summary, object inspection, systems inspection, rebuild-path, and debug-toggle coverage
* `CarrotEditor` builds cleanly with the new inspection panels
* the temporary editor-hosted runtime harness proves the scene/object/system panels against a real loaded `scene.sandbox.town`

The main follow-on work exposed by this milestone is no longer engine inspection plumbing. It is mostly:

* UI toolkit growth for scrollable/clipped inspection surfaces
* eventual separation between authored-scene inspection and runtime-scene inspection
* eventual replacement of the temporary hosted runtime harness with a more intentional editor/runtime integration path

---

## Definition Of Done

At milestone closeout, Carrot should be able to support a workflow like this:

1. run `CarrotSandbox`
2. open `CarrotEditor`
3. inspect the active scene and its world contents
4. change a texture/sprite/audio asset and see reload behavior clearly
5. change a tilemap/scene-related asset and get an explicit rebuild/restart requirement when needed
6. confirm through the scene inspector what the engine actually loaded
7. do all of that without making the editor the owner of runtime truth

---

## Likely Follow-On Work After Milestone 17

Natural follow-up after this milestone would likely be one of:

* save/persistence architecture
* deeper scene/runtime inspection polish
* broader input-routing diagnostics and multiplayer join flow
* eventual optional editor-authoring experiments

But those should come after this milestone proves the inspection and iteration boundary cleanly.
