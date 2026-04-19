# Carrot Game Engine - Milestone 26

**Last Updated:** April 18, 2026
**Title:** Composite/Post-FX and Engine Transition Effects
**Status:** Planned
**Focus:** Build the renderer foundation for bloom, future shafts, and engine-owned fullscreen transition effects, with bloom and battle swirl implemented on the milestone closeout path.

---

## Milestone Goal

Carrot already has two important pieces of direction that should be preserved:

* a deliberate `composite` stage in the renderer
* an engine-owned scene transition overlay system with current styles such as `fade`, `loading_screen`, and `wipe`

That means the next presentation/composite question is no longer:

* does Carrot have any home at all for post-world fullscreen work or transition presentation?

It does.

The next presentation/composite question is:

* whether that home is strong enough for real fullscreen post effects
* whether transition presentation can grow from overlay-style logic into a reusable engine effect system
* whether bloom and battle swirl can land as real engine features instead of hacks
* whether light shafts can be made architecturally ready even if authored/import work still needs a later pass

Milestone 26 exists to turn `composite` and current transition overlays into a durable engine-owned presentation framework.

This milestone is successful if Carrot ends with:

* intermediate render-target and fullscreen-pass infrastructure
* a reusable engine transition-effect system
* the default fade migrated onto that system
* bloom implemented
* battle swirl implemented as the first advanced engine transition effect
* light shafts enabled by the architecture even if final implementation is deferred for authored/import reasons

---

## Scope Summary

Milestone 26 is:

* a composite/post foundation milestone
* an engine transition-effects milestone
* a bloom milestone
* a battle-swirl milestone
* a future-shafts readiness milestone

Milestone 26 is not:

* a full cinematic presentation stack
* a promise that every future post effect lands now
* a requirement that shafts ship before their authored/input contract is ready
* a replacement of the current scene-runtime transition lifecycle

The key rule is:

**Battle swirl should land as the first advanced engine transition effect on a reusable system, not as a one-off special case.**

---

## Why This Milestone Comes Last

Bloom, battle swirl, and shafts all depend on the earlier milestones:

* milestone 23 provides the compute-capable and multi-pass-capable contract
* milestone 24 proves compute-classified renderer data flow in a real shipped slice
* milestone 25 reduces CPU pressure in the world path and gives the renderer a more durable execution model

Only after those foundations exist should Carrot grow:

* fullscreen post passes
* richer transition effects
* broader composite-stage orchestration

Trying to do this earlier would either:

* force special-case renderer plumbing
* smuggle presentation work through the wrong stage
* or overfit the implementation to one effect instead of to the engine's long-term presentation seam

---

## Core Architectural Rule

Carrot should treat transition presentation as an engine-owned effect system layered on top of the existing scene-runtime transition lifecycle.

That means:

* scene runtime remains responsible for transition state and safe activation sequencing
* the renderer/composite layer becomes responsible for effect execution and presentation
* existing fade/wipe/loading-screen behavior should migrate onto the new seam rather than being discarded
* battle swirl should be implemented as one engine effect on that reusable seam

If the milestone only adds bloom and a hardcoded battle-swirl branch, it has not gone far enough.

---

## Primary Deliverables

### 1. Intermediate Render Target and Fullscreen Pass Infrastructure

Carrot should gain the core substrate needed for fullscreen post and transition work.

Required outcomes:

* intermediate render targets for world/composite/post processing
* fullscreen pass execution utilities
* explicit resource lifetime and pass-boundary behavior for the milestone slice

### 2. Engine Transition Effect Contract

The current scene transition overlay seam should grow into a reusable effect system.

Required outcomes:

* named engine-owned transition effects
* effect parameters/options where needed
* preserved default/per-request override behavior
* compatibility with current fade, wipe, and loading-screen presentation

### 3. Default Fade Migration

The current fade behavior should become the baseline implementation on the new transition system.

Required outcomes:

* no regression for existing scene-transition behavior
* default fade remains the baseline engine transition
* current transition lifecycle and diagnostics remain truthful

### 4. Bloom Implementation

Carrot should ship one real post effect on the new infrastructure.

Required outcomes:

* bloom implemented through the new composite/post path
* no one-off hack path for bloom
* renderer-level ownership of the feature

### 5. Battle Swirl Implementation

Carrot should ship one real advanced transition effect.

Required outcomes:

* battle swirl implemented as an engine-owned transition effect
* gameplay can request it as a transition override instead of the default fade
* effect execution stays renderer/composite-owned rather than gameplay-owned

### 6. Light Shaft Readiness

Carrot should leave the milestone architecturally ready for shafts even if authored/input work is still pending.

Required outcomes:

* renderer substrate suitable for shaft-style post work
* defined expectations for mask/occlusion/source inputs
* no second architecture milestone needed before shafts can be added

### 7. Diagnostics and Runtime Truth Preservation

Carrot should preserve the current discipline of queryable transition/runtime truth even as transition presentation gets richer.

Required outcomes:

* transition diagnostics remain visible and meaningful
* active effect state can be surfaced where appropriate
* docs remain honest about what is shipped versus only architecturally enabled

---

## Ticket Breakdown

### Ticket 26.1 - Composite Target and Fullscreen Pass Infrastructure

**Priority:** P0
**Outcome:** The renderer has the core substrate for fullscreen post and transition effects.

#### Why

The current `composite` stage is the right architectural home, but it needs a stronger substrate than "single overlay color or a few immediate quads."

#### Scope

Add:

* intermediate render targets
* fullscreen pass orchestration
* explicit resource handling suitable for the milestone slice

#### Acceptance Criteria

* the renderer can run at least one real fullscreen pass over an intermediate target
* resource transitions and lifetime handling are explicit for the milestone slice
* composite-stage ownership remains clear

### Ticket 26.2 - Engine Transition Effect Contract

**Priority:** P0
**Outcome:** Scene transition presentation grows from overlay-style logic into a reusable engine effect system.

#### Why

Carrot already has real transition overlay options and override plumbing in scene runtime.
That seam should be extended, not replaced.

#### Scope

Extend the current transition presentation model so that gameplay/runtime can request:

* default transition behavior
* named engine transition effects
* effect-specific override parameters where justified

#### Acceptance Criteria

* game code can request default transition behavior or a named override
* the scene-runtime lifecycle remains responsible for safe transition sequencing
* the contract supports fade, wipe, loading-screen, and battle swirl as engine effects

### Ticket 26.3 - Default Fade Migration

**Priority:** P0
**Outcome:** Existing fade behavior is implemented through the new transition-effect system.

#### Scope

Migrate the current default fade onto the new engine transition contract without regressing current runtime scene flow.

#### Acceptance Criteria

* current fade behavior still works
* existing transition requests remain safe and understandable
* the new effect system proves it can host current and future effects together

### Ticket 26.4 - Bloom Implementation

**Priority:** P0
**Outcome:** Carrot ships bloom as the first real engine-owned post effect on the new infrastructure.

#### Scope

Implement a practical bloom path appropriate to Carrot's current renderer scale.

#### Acceptance Criteria

* bloom is implemented and functional in the milestone slice
* bloom runs on the composite/post infrastructure rather than a one-off hack path
* the renderer can intentionally enable or disable bloom behavior

### Ticket 26.5 - Battle Swirl Transition Effect

**Priority:** P0
**Outcome:** Carrot ships a battle-swirl-style engine transition effect that gameplay can request as an override to the default fade.

#### Scope

Implement battle swirl as:

* an engine-owned transition effect
* renderer/composite-executed presentation logic
* a gameplay-requestable transition override

#### Acceptance Criteria

* game code can request battle swirl as a transition override
* scene runtime still owns the transition lifecycle and safe activation timing
* battle swirl is implemented as the first advanced effect on the reusable transition seam

### Ticket 26.6 - Light Shaft Readiness Contract

**Priority:** P1
**Outcome:** Carrot's renderer and authored-data seams are ready for shafts even if the final effect lands later.

#### Scope

Define:

* the renderer inputs shafts will need
* mask/occlusion expectations
* any authored or imported light-source data the effect depends on

#### Acceptance Criteria

* the renderer does not need another architecture milestone before shafts can be added
* authored/import dependencies are documented clearly
* if shafts are not implemented yet, it is because content/input work is still pending rather than because the renderer remains unready

### Ticket 26.7 - Transition and Post-FX Diagnostics

**Priority:** P1
**Outcome:** Carrot preserves visible runtime truth as transition presentation and post work get richer.

#### Acceptance Criteria

* transition diagnostics remain truthful
* active effect identity/state can be surfaced where appropriate
* docs explain clearly which effects are implemented and which are only architecturally enabled

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. intermediate render-target and fullscreen-pass infrastructure
2. a reusable engine transition-effect system built on the current scene-runtime transition lifecycle
3. default fade migrated onto that system
4. bloom implemented
5. battle swirl implemented as a gameplay-requestable transition override
6. architectural readiness for later light shafts

If those land cleanly, the milestone succeeds even if:

* the first bloom implementation is intentionally modest
* shafts remain unimplemented because authored/import dependencies are not ready yet
* broader future post effects are still later work

---

## Closeout Criteria

Milestone 26 is complete when:

* Carrot has reusable composite/post infrastructure
* engine-owned transition effects exist as a real system
* default fade runs on that system
* bloom is implemented
* battle swirl is implemented and gameplay-requestable as a transition override
* light shafts are enabled by the architecture even if final implementation is deferred for authored/import reasons
