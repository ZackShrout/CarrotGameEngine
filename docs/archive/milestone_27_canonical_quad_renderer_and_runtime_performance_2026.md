# Carrot Game Engine - Milestone 27

**Last Updated:** April 23, 2026
**Title:** Canonical Quad Renderer and Runtime Performance
**Status:** Completed
**Focus:** Establish Carrot's durable 2D quad-rendering architecture, replace provisional quad execution with the canonical long-term path, formalize profiling/performance truth, and preserve future composite/offscreen growth without expanding into unrelated renderer scope.

---

## Milestone Goal

Carrot now has a real renderer.
That is no longer the question.

The next renderer question is:

* whether the current quad path is the one Carrot actually intends to keep
* whether renderer-facing runtime costs are being measured honestly
* whether 2D game scenes can hold display refresh cleanly without mystery churn
* whether the renderer has a formal offscreen/composite substrate instead of a collection of narrowly sufficient stage-local solutions

Milestone 27 exists to end the "temporary but good enough" phase of Carrot's 2D quad renderer.

This milestone is successful if Carrot ends with:

* a clearly canonical quad renderer path that the engine intends to build on long-term
* instanced quad execution as the default renderer direction for the validated 2D slice
* profiling and diagnostics that separate renderer cost from other runtime costs
* formal render-target / offscreen ownership where the current 2D/composite path actually needs it
* preserved backend parity across Vulkan, Metal, and DirectX 12 for the milestone slice
* preserved or improved readiness for future shaft-light / richer composite work without shipping those features now

This milestone is not about finishing the renderer forever.
It is about making Carrot's real 2D renderer honest, durable, measurable, and worth building on.

---

## Scope Summary

Milestone 27 is:

* a canonical 2D renderer milestone
* an instanced quad execution milestone
* a runtime performance-truth milestone
* an offscreen/composite substrate milestone
* a renderer diagnostics and parity-hardening milestone

Milestone 27 is not:

* a 3D renderer milestone
* a shadows milestone
* a spot-light milestone
* a shaft-light implementation milestone
* a broad lighting-model expansion milestone
* a giant post-effects milestone
* a material-system rewrite

The key rule is:

**Milestone 27 should ship Carrot's canonical 2D quad renderer path, not attempt to complete every future renderer feature.**

## Closeout Summary

Milestone 27 is complete.

Carrot now has:

* one clearly canonical renderer-owned quad path for the validated 2D slice
* instanced quad execution as the default shared direction across Vulkan, Metal, and DirectX 12
* explicit backend-owned upload-ring structure aligned around the shared `Core/Memory/Ring` primitive
* formal render-target and offscreen routing for the current composite and transition slice
* profiling and CSV export surfaces that separate renderer cost from broader frame/runtime cost
* backend-facing presentation diagnostics that make profiling provenance explicit instead of inferred

The most important closeout finding from the milestone is that the renderer itself is performing very well.
Across the representative Metal and Vulkan profiling passes used during closeout, world render cost remained very low and stable, while total frame time stayed dominated by presentation cadence rather than meaningful renderer-side pressure.

That means Milestone 27 successfully answered the milestone's most important truth question:

**Carrot's renderer is no longer in the \"temporary but good enough\" phase; it is now a durable, measurable, performant 2D renderer worth building on.**

## Closeout Notes

* the profiling/tooling work proved more valuable than additional renderer-side optimization work
* further GPU-side culling complexity for the current quad draw path was intentionally not pursued after the closeout data showed little meaningful renderer-side juice left to squeeze
* startup-time presentation policy now lives in engine config, while runtime profiling capture remains an explicit separate concern
* `graphics.present_sync` defaults to `true` when omitted from engine config so the engine keeps safe/predictable presentation behavior by default

## Validation Snapshot

Milestone 27 closeout validation established that:

* Vulkan and Metal both cleanly execute the canonical milestone slice
* DirectX 12 remained structurally aligned with the same shared renderer path during the refactor and stabilization work
* representative profiling captures can now identify backend, requested sync policy, backend-reported presentation-mode selection, and frame-stage timing in one export
* current renderer-facing performance questions can now be answered with measurement instead of intuition

---

## Why This Milestone Comes Next

Carrot is now at the point where renderer uncertainty has become a structural drag:

* performance concerns are real enough that "it is probably fine" is no longer acceptable
* some current frame-rate observations are likely presentation-capped and therefore not exposing true headroom
* the engine still needs a cleaner line between current world/UI/composite behavior and the renderer substrate they depend on
* future work such as richer post/composite behavior should not be built on a quad path that Carrot already expects to replace

This makes Milestone 27 the right next milestone because it can answer several important questions at once:

* what the long-term quad path actually is
* where current frame time is really going
* whether the renderer is paying unnecessary CPU cost in quad extraction/submission/execution
* whether the current 2D/composite substrate is robust enough for the next renderer phase

If Carrot tries to push farther into more renderer features before answering those questions, it risks compounding uncertainty instead of reducing it.

---

## Core Architectural Rule

Carrot's gameplay-facing and world-facing systems should submit 2D render intent.
The renderer should own how that intent becomes durable quad execution.

For the validated milestone slice, that means:

* the renderer should own the canonical quad instance representation
* the renderer should own final execution details such as instance packing, batching, draw dispatch, and render-target routing
* the engine should stop treating CPU-expanded quad geometry as the assumed permanent execution model
* the renderer should expose honest performance truth rather than forcing contributors to guess where frame time went

If the milestone improves raw speed temporarily but leaves the renderer's internal ownership unclear, it has not gone far enough.

## Locked Implementation Direction

Milestone 27 now adopts the following implementation policy for the renderer refactor.

These decisions are no longer open questions for the milestone:

* Carrot is building one backend-neutral canonical renderer, not a Vulkan-shaped renderer
* Vulkan is the first backend that will fully implement the new canonical bucketed instanced quad path
* Metal and DirectX 12 may be temporarily stubbed during the refactor as long as they continue to compile and fail explicitly instead of degrading silently
* backend parity is restored before milestone closeout, but does not need to be preserved continuously during the structural rewrite
* compatibility layers should not be kept merely because the old path existed; if current renderer-facing or public APIs obstruct the canonical path, Milestone 27 is allowed to replace them cleanly

That means this milestone is allowed to be a prolonged renderer surgery milestone rather than a short compatibility-preserving increment.

The rule is:

**Carrot should prefer the canonical renderer over temporary compatibility if the two are in conflict.**

---

## Performance Success Criteria

Milestone 27 should define performance in two ways:

### 1. Presentation-Capped Success

Carrot should:

* hold the active presentation refresh cap stably in representative 2D gameplay scenes
* avoid mystery churn that causes visible pacing instability even when the average frame rate looks acceptable
* remain predictable across the validated Vulkan / Metal / DirectX 12 slice

### 2. Uncapped Profiling Truth

Carrot should:

* expose an uncapped or de-paced profiling mode suitable for measuring actual renderer/runtime headroom
* report frame time in milliseconds, not just headline FPS
* separate renderer cost from collision, world update, UI, scene runtime, and presentation pacing where practical

### 3. Benchmark Scene Discipline

Milestone validation should use at least three benchmark classes:

* a light gameplay scene
* a representative real gameplay scene
* a stress scene with intentionally heavy world quad, tilemap, UI, and runtime churn

Milestone 27 should not rely on a single flattering benchmark scene.

### 4. Frame-Time-Oriented Targets

Carrot should aim for:

* representative light scenes comfortably below `4 ms/frame` in uncapped profiling on the development reference machines
* representative real gameplay scenes around `4-6 ms/frame` where practical
* heavy stress scenes that stay within a predictable budget instead of degrading opaquely

These are guidance targets, not false precision promises across all hardware.
The main engineering requirement is that the renderer exposes enough truth to know whether it is structurally on track.

---

## Shaft-Light Readiness Rule

Milestone 27 intentionally does **not** implement shaft lighting.

That non-goal is deliberate because shaft lighting is exactly the kind of adjacent renderer work that could distort this milestone's scope.

However, current readiness for future shaft-light work must not simply disappear.

Milestone 27 should therefore preserve or consciously migrate:

* offscreen/render-target ownership needed by future composite-style effects
* world-to-composite data flow seams that future shafts may need
* pass-boundary clarity for future mask/occlusion/source inputs
* backend parity expectations for richer composite/post passes

If the canonical quad renderer requires changing those seams, that is acceptable.
If it silently erases them, that is a regression.

---

## Primary Deliverables

### 1. Canonical Quad Instance Path

Carrot should adopt a renderer-owned quad instance representation as the real long-term path for the milestone slice.

Required outcomes:

* a shared instance-facing representation for quad rendering where appropriate
* world/UI/composite-facing quad submission routes that can map into that canonical path
* explicit retirement or relegation of legacy CPU-expanded geometry paths where they are no longer the default

The validated milestone slice for that path is:

* world textured quads
* world text quads
* UI textured and text quads
* overlay debug textured and text quads
* log console textured and text quads
* composite textured and solid fullscreen/quad work that belongs to the current 2D/composite path

The milestone should treat that whole slice as the target architecture, not as unrelated mini-renderers.

### 2. Instanced Quad Execution

Carrot should execute the validated quad slice through instanced rendering instead of treating per-frame CPU-expanded quad geometry as the permanent approach.

Required outcomes:

* instance-buffer-driven quad execution for the milestone slice
* reduced dependence on fully expanded CPU-authored quad vertex/index streams
* preserved correctness for layering, ordering, presentation routing, and current renderer stages

### 3. Performance Instrumentation and Runtime Truth

Carrot should stop relying on "it feels slow" as the primary signal.

Required outcomes:

* frame-time instrumentation
* stage-aware renderer diagnostics
* profiling surfaces that can distinguish renderer cost from non-renderer cost
* uncapped or de-paced measurement support for true headroom analysis

### 4. Render-Target / Offscreen Formalization

Carrot should formalize the render-target behavior actually needed by the live 2D/composite path.

Required outcomes:

* explicit render-target ownership for the validated milestone slice
* honest offscreen target lifetime/sizing behavior
* cleaner fullscreen/composite routing on top of those targets

### 5. Backend Parity and Regression Hardening

The canonical quad renderer path should remain a shared supported renderer slice, not a one-backend-first experiment.

Required outcomes:

* Vulkan / Metal / DirectX 12 validation against the live milestone slice
* null-backend or shared regression tests where possible
* updated docs/checklists describing the current supported renderer truth

---

## Ticket Breakdown

### Ticket 27.1 - Canonical Quad Path Definition and Renderer Contract Cleanup

**Priority:** P0
**Outcome:** Carrot has one clearly defined long-term quad renderer path for the validated 2D slice.

#### Why

The engine has already proven several useful renderer steps, but the next phase needs clarity about which quad path is canonical and which are only transitional or narrowly scoped.

Without that line:

* performance work can optimize the wrong path
* future renderer work can accidentally build on provisional assumptions
* contributors cannot tell which execution model is intended to survive

#### Scope

Define and implement the canonical quad contract for the validated milestone slice:

* renderer-owned quad instance structure
* required sort/bucket/material/texture identity carried by that structure
* routing rules for world/UI/composite quad submissions where they share the path
* explicit handling of any stage-local exceptions that should remain separate for now

#### Acceptance Criteria

* Carrot has a documented canonical quad path
* the renderer's internal ownership of quad execution is clearer after the change
* obsolete or provisional legacy paths are clearly demoted, retired, or documented as non-canonical

#### Locked Direction

For Milestone 27, "canonical quad path" means:

* one renderer-owned quad instance contract shared across the validated 2D slice where stage semantics allow it
* one renderer-owned bucket/build/dispatch path for quad execution
* no equal-status split between world indirect execution and non-world CPU-expanded direct execution
* gameplay-facing draw APIs may remain intent-shaped, but renderer-facing execution APIs may be replaced outright if needed

### Ticket 27.2 - Instanced Quad Execution for the Validated Slice

**Priority:** P0
**Outcome:** The live quad path executes through instance-driven rendering for the milestone slice.

#### Why

For a 2D-focused engine like Carrot, continuing to treat CPU-expanded per-frame quad geometry as the default long-term execution model is not the right architectural endpoint.

Instanced quad rendering should become the normal execution path for the validated slice.

#### Scope

Implement the instanced path and migrate the milestone slice onto it:

* stable quad mesh or equivalent shared geometry basis
* instance buffer layout and upload path
* draw recording and binding flow for instanced quads
* preservation of stage-local semantics such as layering/order and presentation routing

#### Acceptance Criteria

* the validated milestone slice uses instanced quad rendering as the default execution path
* the renderer no longer depends primarily on fully expanded per-frame quad geometry there
* output correctness is preserved for real engine content

#### Backend Transition Rule

Milestone 27 may sequence backend implementation as follows:

* Vulkan first: full implementation and validation target during the initial refactor passes
* Metal and DirectX 12 secondarily: compile-preserving stubs are acceptable while the shared contract is still moving
* re-enable one suspended backend at a time after Vulkan stabilizes
* choose the second restored backend based on which backend best tests the abstraction rather than by habit

### Ticket 27.3 - Renderer Extraction, Batching, and Submission Audit

**Priority:** P0
**Outcome:** Carrot identifies and removes major CPU-side waste in quad extraction and submission.

#### Why

A faster draw path is not enough if the engine still burns frame time earlier in the same pipeline.

This milestone needs an honest audit of:

* quad extraction
* sorting/bucketing
* instance packing
* texture/material grouping
* stage submission overhead

#### Scope

Add diagnostics and perform the structural cleanup needed so the canonical path is not undermined by avoidable upstream churn.

#### Acceptance Criteria

* major per-frame CPU waste in the validated path is measured and reduced
* batching/submission behavior is more understandable after the milestone
* the renderer exposes enough truth to know whether remaining frame cost is draw-side, extraction-side, or elsewhere

### Ticket 27.4 - Runtime Performance Instrumentation and Profiling Mode

**Priority:** P0
**Outcome:** Carrot can measure real frame cost instead of relying on presentation-capped FPS alone.

#### Why

Current observations strongly suggest that some measurements are capped by presentation cadence or compositor behavior.
That makes raw FPS insufficient as the milestone's primary truth source.

#### Scope

Add profiling support appropriate for the current engine:

* uncapped or de-paced profiling mode where practical
* stage/frame timing
* renderer and non-renderer timing surfaces
* benchmark scene discipline for milestone validation

#### Acceptance Criteria

* Carrot can report frame time meaningfully in the milestone slice
* capped and uncapped results are distinguishable
* contributors can identify whether slowdowns come from renderer work, collision/world work, scene runtime, UI, or presentation

### Ticket 27.5 - Render-Target and Offscreen Pass Formalization

**Priority:** P0
**Outcome:** The live 2D/composite path uses a more explicit render-target/offscreen contract instead of relying on ad hoc or narrowly sufficient assumptions.

#### Why

The canonical quad path should not sit on top of renderer substrate that Carrot already expects to replace.

The engine now needs a more formal answer to:

* where offscreen rendering lives
* who owns render-target lifetime/sizing
* how current fullscreen/composite work routes through those targets

#### Scope

Formalize only what the current milestone slice actually needs:

* explicit render-target ownership
* honest resizing/lifetime rules
* current offscreen/composite routing for the validated 2D slice

#### Acceptance Criteria

* the milestone slice has a formalized render-target/offscreen contract
* current composite work still functions on top of it
* the resulting structure is better prepared for future shaft-light/post work without implementing those effects now

### Ticket 27.6 - Legacy Path Retirement and Scope Discipline

**Priority:** P1
**Outcome:** Carrot exits the milestone with one clear default path rather than indefinitely carrying two equal-status quad architectures.

#### Why

It is acceptable for some narrow fallback/debug paths to remain.
It is not healthy for the engine to act as though the old path and the new path are equally intended long-term answers.

#### Scope

Retire, demote, or quarantine legacy quad execution paths that no longer deserve first-class status in the validated slice.

#### Acceptance Criteria

* the engine has one clearly preferred quad path after the milestone
* remaining exceptions are explicit and justified
* milestone docs describe that truth honestly

### Ticket 27.7 - Backend Parity Validation and Documentation Refresh

**Priority:** P1
**Outcome:** The canonical quad path is validated as part of Carrot's real supported backend slice.

#### Why

Milestone 27 should not trade architectural clarity for backend uncertainty.

#### Scope

Update validation and docs for the new canonical path:

* null/shared regression tests where feasible
* manual native validation on Vulkan / Metal / DirectX 12
* updated support notes/checklists

#### Acceptance Criteria

* the canonical quad path is documented as a real supported slice
* backend parity expectations remain explicit
* validation is written down instead of assumed

#### Sequencing Clarification

This ticket intentionally closes the temporary Vulkan-first refactor window.

That means:

* Vulkan being first to full implementation is acceptable
* temporary Metal/DirectX 12 stubbing is acceptable during the milestone body
* milestone closeout still requires all three backends to participate in the canonical shared path again

---

## Non-Goals

Milestone 27 should explicitly not grow into:

* shaft-light implementation
* spot lights
* shadows
* broader world-light model redesign
* 3D geometry or materials
* broad text/UI/renderer unification beyond what the canonical quad path actually needs
* every possible post effect that might someday use the new substrate

These are all legitimate future topics.
They are not the purpose of this milestone.

---

## Validation Expectations

Milestone 27 should not be considered complete until it validates:

* real engine scenes, not just synthetic tests
* capped presentation behavior and uncapped profiling behavior separately
* backend parity on the supported native slice
* that the canonical quad path remains structurally compatible with current composite behavior
* that current future-facing readiness for shaft-light/composite growth was preserved or consciously migrated

During the transition period, milestone progress notes should explicitly distinguish:

* the backend currently used as the active implementation truth source
* backends that still compile but are temporarily stubbed for the new path
* backends that have been re-enabled against the canonical contract

---

## Success Criteria

Milestone 27 is succeeding when:

* Carrot can honestly say what its canonical 2D quad renderer path is
* the validated slice uses instanced quad rendering by default
* renderer performance is measured through frame-time truth rather than guessed from capped FPS alone
* current scenes hold refresh reliably with materially improved predictability
* the engine's 2D/composite substrate is clearer after the milestone, not murkier
* future shaft-light readiness is still present even though shaft lighting itself remains out of scope

That is enough for Milestone 27 to be a major renderer milestone without pretending it is the end of renderer evolution.

## Closeout Status

Milestone 27 should now be treated as a closed milestone record rather than an active implementation plan.
Future renderer work should build from the resulting canonical renderer, profiling, and render-target foundations established here rather than reopening this milestone's architecture questions by default.
