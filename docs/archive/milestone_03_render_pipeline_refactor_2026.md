# Carrot Game Engine - Milestone 03

**Title:** Render Pipeline Refactor
**Status:** Complete
**Focus:** Replace the current single-pass frame flow with a clearer multi-stage render pipeline that can support world rendering, debug rendering, UI, and future lighting/composite work without collapsing into one pass.

---

## Milestone Goal

Milestone 02 tightened the current playable scene flow and validated what the engine can already do well.

The next major structural limiter is the render frame itself.

Right now Carrot still effectively renders through one pass, which creates several problems:

* debug text/overlay rendering is coupled too closely to world rendering
* there is no clean place for future UI rendering
* there is no clean place for full-screen transition effects
* there is no clean growth path for lighting/composite work
* the frame structure is too flat for future hybrid 2D/3D rendering needs

This milestone focuses on fixing that frame-architecture ceiling before more rendering and gameplay features pile onto it.

## Current Progress

Completed so far:

* Ticket 1 - frame stage and pass boundary refactor
* Ticket 2 - debug text and overlay rendering separated from the world stage
* Ticket 3 - full-screen overlay/composite hook
* Ticket 4 - render-order policy scaffolding with a validated `anchor_bottom_y` proof
* Ticket 5 - documentation, verification, and milestone closeout

Milestone 03 shipped these concrete outcomes:

* the engine loop now has explicit `render_world()`, `render_ui()`, and `render_debug()` hooks
* the renderer owns ordered frame stages: `world`, `ui`, `composite`, and `overlay_debug`
* all three graphics backends record stage-aware textured-quad submissions safely
* debug overlay/text rendering is no longer coupled to the world path
* a small engine-level full-screen overlay primitive exists for future fade/flash/tint work
* render ordering now has renderer-owned sort intent instead of only flat explicit order
* a narrow opt-in `anchor_bottom_y` proof was validated in the sandbox for actor/tile-object style sorting

---

## Scope Summary

This milestone is about **frame architecture**, not “ship every future render feature now.”

Carrot’s long-term render direction is a **single deferred-lighting-oriented architecture** for the engine as a whole, including future 2D/3D hybrid games.

This milestone does **not** implement deferred lighting itself.
It does make sure the frame structure stops fighting that future.

It should create a render pipeline shape that can later support:

* world pass separation
* debug pass separation
* UI pass separation
* full-screen overlay/composite effects
* future lighting and shadow-related work

It does **not** need to deliver full lighting, shadows, or a full UI toolkit in this milestone.

An important design rule for this milestone:

* a frame stage should not be assumed to map permanently to exactly one backend render pass
* the world stage must be free to grow into a multi-pass internal pipeline later

---

## Ticket 1 - Frame Stage and Pass Boundary Refactor

**Priority:** P0
**Outcome:** Carrot has an explicit multi-stage frame structure instead of a de facto single-pass world render path.

### Why

The current frame shape is now an architectural bottleneck.

Without a cleaner stage/pass model, future work such as UI, lighting, screen fades, and better debug rendering will either be awkward or will push more policy into places that should stay simple.

### Scope

Refactor the high-level renderer flow so the frame can express separate responsibilities clearly.

Suggested first structure:

* world render stage
* reserved UI stage, even if only lightly used at first
* debug/text overlay stage

Current Ticket 1 stage contract:

* `world` executes in world-camera space
* `ui` executes in full render-target pixel space
* `composite` executes in full render-target pixel space
* `overlay_debug` executes in resolved viewport-local pixel space
* `overlay_debug` renders after `ui` so diagnostics stay visible over all game presentation

Design constraint:

* the `world` stage is a stage identity, not a promise of “one pass forever”
* later milestones should be able to evolve it into a more complex internal pipeline without redesigning the whole frame model

### Likely Touch Points

* [Renderer.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Renderer/Renderer.cpp)
* [Renderer.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Renderer/Renderer.h)
* [RendererService.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Renderer/RendererService.cpp)
* render-facing game/debug overlay wiring as needed

### Acceptance Criteria

* The frame has explicit render stages or pass ownership.
* World rendering no longer implicitly owns every screen-space draw.
* The new structure is clear enough to host future UI and composite work.
* The frame model does not assume the world stage will remain a single-pass renderer permanently.
* Stage-space ownership is explicit:
  * `world` uses the active world camera
  * `ui` uses render-target pixel space
  * `composite` uses render-target pixel space
  * `overlay_debug` uses viewport-local pixel space

---

## Ticket 2 - Move Debug Text and Overlay Rendering Out of the World Pass

**Priority:** P0
**Outcome:** Debug text and related overlays render in their own later frame stage rather than sharing the world pass by accident.

### Why

Debug text is not world geometry.

Keeping it in the same render path creates the wrong long-term ownership model and makes future UI/composite layering harder.

### Scope

Split the current debug text/overlay draw path into a dedicated overlay stage that renders after world content.

### Likely Touch Points

* [DebugOverlay.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Debug/DebugOverlay.cpp)
* [Renderer.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/Renderer/Renderer.cpp)
* any render submission path currently coupling debug draws to world draws

### Acceptance Criteria

* Debug text/overlay rendering is no longer part of the world draw path.
* The frame ordering is explicit and easy to reason about.
* Existing debug output still works.

---

## Ticket 3 - Full-Screen Overlay / Composite Hook

**Priority:** P1
**Outcome:** The render pipeline exposes a clean engine-level place for a final full-screen color/composite overlay.

### Why

A small full-screen overlay hook pays off immediately and supports several future features:

* fade-out / fade-in transitions
* damage flashes
* screen tinting
* future post/composite experiments

This should be an engine primitive, not yet a full gameplay transition module.

### Scope

Add a minimal render-stage capability to draw a full-screen quad/overlay color after world/debug work.

### Acceptance Criteria

* The engine can render a configurable full-screen color overlay in a late frame stage.
* The hook is general-purpose rather than tied only to scene transitions.
* The overlay hook lives below `overlay_debug` so engine diagnostics remain readable over composite presentation.

---

## Ticket 4 - Render Ordering and Layering Prep

**Priority:** P1
**Outcome:** The render pipeline is better prepared for future 2D layering and depth-sort work.

### Why

Milestone 03 does not need to fully solve fence/bridge/occlusion behavior, but the render pipeline should stop making that harder.

### Scope

Clarify where render ordering decisions belong and prepare the frame structure for:

* layered 2D world rendering
* future per-object ordering refinement
* future world-vs-overlay-vs-UI separation

Current preparation in place:

* textured-quad submissions now carry an explicit render-order mode
* the renderer owns the sort policy for those modes
* current defaults remain stable while leaving room for future y-sort / anchor-driven ordering work
* a narrow opt-in `anchor_bottom_y` path has been validated for actor/tile-object style content in the sandbox

Important limitation:

* this is not full 2D layering support yet
* it does not claim to solve roofs, bridges, fences, or all occlusion authoring cases
* current validation is intentionally small and focused on proving the renderer path

### Acceptance Criteria

* The refactored pipeline does not box Carrot into a flat one-layer world model.
* The architectural direction for future 2D layering is clearer after this pass.
* At least one small opt-in ordering proof has been validated in real sandbox content without expanding into a full layering feature set.

---

## Ticket 5 - Tests, Verification, and Documentation

**Priority:** P2
**Outcome:** The render pipeline refactor is documented and regression-resistant enough to build on confidently.

### Why

Renderer architecture changes are easy to regress if they are only held in working memory.

### Scope

Document the new frame structure and add whatever verification is practical at this stage.

Suggested outputs:

* architecture notes update for frame-stage ownership
* small render-path verification where feasible
* sandbox/manual verification notes if automated graphics validation is still too early

### Acceptance Criteria

* The new frame architecture is documented.
* The intended stage ordering is easy to discover later.
* The current scope and limitations of the ordering proof are documented clearly enough that later milestones can build on it without confusion.

---

## Explicit Non-Goals

This milestone should **not** expand into:

* full 2D lighting
* shadows
* full UI toolkit implementation
* full hybrid 2D/3D rendering
* full gameplay transition presentation modules

Those become much easier after the frame architecture is in place.

---

## Why This Milestone Comes Next

After Milestone 02, Carrot has stronger gameplay flow, but its render frame is still too structurally narrow.

This render-pipeline refactor is the highest-leverage next step because it unlocks:

* future UI work
* future lighting/composite work
* cleaner debug rendering
* better separation between engine rendering primitives and game presentation policy

It is a more engine-expanding milestone than another pure tightening pass.

---

## Completion Summary

Milestone 03 is complete.

It did not implement lighting, a UI toolkit, or full 2D layering.
It did establish the frame architecture those systems can now grow on top of:

* explicit frame stages with clear ownership
* backend-safe stage recording
* dedicated debug and composite layers
* a renderer-owned ordering model that can evolve beyond flat explicit order

The milestone also validated one small but important reality check:

* bottom-anchor sorting is viable for actor/tile-object style content

That proof was intentionally kept narrow so the engine gained confidence without drifting into a larger layering milestone too early.
