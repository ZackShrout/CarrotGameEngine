# Carrot Game Engine - Milestone 20

**Last Updated:** April 15, 2026
**Title:** Asset Iteration Structural Refresh and Dependency Reasoning
**Status:** Proposed
**Focus:** Turn Carrot's first-pass runtime iteration work into a more permanent engine capability by making dependency reasoning, refresh actions, and rebuild requirements clearer, safer, and more explainable.

---

## Milestone Goal

Milestones 12, 13, 17, and 18 establish the right base:

* imported/cooked artifacts are real
* asset diagnostics and runtime iteration status are real
* selective reload behavior is real
* rebuild-required assets are no longer hidden behind false hot-reload promises
* scene runtime now has a stronger structural refresh contract

That means the next asset-iteration bottleneck is no longer:

* can the engine detect some changes?
* can the engine reload some safe asset types?
* can the engine show basic status in diagnostics and tooling?

It can.

The next bottleneck is:

* how clearly the engine reasons about dependencies
* whether refresh actions are explainable instead of surprising
* whether structural asset changes map cleanly onto runtime actions
* whether diagnostics describe why the engine chose reload, rebuild, or no immediate action

Milestone 20 exists to make runtime iteration a trustworthy engine behavior rather than a thin collection of per-asset exceptions.

This milestone is successful if Carrot ends with:

* stronger dependency reasoning around runtime assets
* clearer mapping from change detection to runtime action
* better diagnostics for invalidation and refresh decisions
* less ambiguity around rebuild-required asset classes
* iteration behavior that feels explicit, honest, and maintainable

---

## Scope Summary

Milestone 20 is:

* an asset-iteration architecture milestone
* a dependency reasoning milestone
* a diagnostics milestone
* a refresh action policy milestone

Milestone 20 is not:

* a promise that every asset hot reloads live
* a giant editor feature milestone
* a packaging/distribution milestone
* a save/persistence milestone
* an asset authoring UX milestone

The key rule is:

**Iteration should be honest and explainable, not magical.**

---

## Why This Milestone Comes Next

Carrot already has enough runtime iteration behavior that ambiguity is becoming more expensive than missing coverage.

Current strengths:

* asset iteration status exists
* reload policy categories exist
* live reload exists for safe asset slices
* scene rebuild flow exists for structural cases
* editor/runtime diagnostics already surface meaningful asset status

Current risks:

* dependency reasoning is still comparatively narrow
* the engine may know that "something changed" without clearly surfacing why a particular action was chosen
* structural changes can still feel like policy islands instead of one coherent refresh model
* later asset types will become harder to integrate cleanly if dependency reasoning remains too ad hoc

This is the right moment to turn iteration into a stronger engine subsystem rather than letting it become a patchwork of special cases.

---

## Core Architectural Rule

Refresh behavior should follow dependency truth, not convenience folklore.

That means:

* the engine should know which authored inputs and cooked outputs a runtime asset depends on
* the engine should know whether those changes affect:
  * leaf runtime data
  * scene/world structure
  * layout/presentation assumptions
* diagnostics should describe the reasoning path, not just the final status

If the engine can only explain its behavior as "this asset type currently does X because that was easy," the iteration model is not mature enough for milestone closeout.

---

## Primary Deliverables

### 1. Stronger Dependency Tracking and Reasoning

Carrot should deepen its understanding of what invalidates what.

Required outcomes:

* clearer dependency relationships for current asset types
* stronger reasoning about source file changes versus manifest changes versus cooked-artifact state
* better ability to explain which dependency changed and why it mattered

### 2. Refresh Action Decision Model

The engine should make refresh actions feel like deliberate outcomes of policy and dependency truth.

Required outcomes:

* clearer action categories such as:
  * live reload
  * refresh on next use
  * manual refresh
  * rebuild current scene
  * restart-required
* stronger mapping between dependency shape and selected action
* fewer ambiguous "tracked, but unclear what to do" cases

### 3. Diagnostics That Explain Decisions

Iteration diagnostics should answer:

* what changed
* how the engine noticed
* what the asset's current reload/refresh policy is
* what the engine attempted
* why a rebuild or restart was required instead of a live reload

Required outcomes:

* richer status fields where appropriate
* stronger log wording
* stronger editor/runtime explanation surfaces

### 4. Structural Asset Refresh Integration

Asset iteration work should connect cleanly to the stronger scene rebuild contract from milestone 18.

Required outcomes:

* structural asset changes can feed into rebuild-required runtime actions more cleanly
* diagnostics explain when current scene rebuild is the correct engine action
* structural refresh does not rely on vague user intuition

### 5. Regression Safety and Policy Honesty

The engine should prefer a narrower, truthful iteration model over a wider, misleading one.

Required outcomes:

* preserve safe live reload where it is already real
* do not overclaim unsafe live reload for structural or high-risk assets
* expand coverage only where the runtime contract remains honest

---

## Ticket Breakdown

### Ticket 20.1 - Asset Dependency Surface Audit

Review current asset systems and identify the dependency information already available versus what is still implicit.

Deliverables:

* clearer dependency model for current asset kinds
* cleanup of any inconsistent dependency reporting
* doc updates reflecting the current reasoning model

### Ticket 20.2 - Refresh Action Mapping Cleanup

Make the engine's chosen action more explicit and more consistent.

Deliverables:

* stronger mapping from policy and dependency shape to runtime action
* reduced ambiguity between manual refresh, rebuild-required, and restart-required cases

### Ticket 20.3 - Richer Iteration Diagnostics

Improve the engine's ability to explain its decisions.

Deliverables:

* richer invalidation and refresh result data
* better log messages
* better structured diagnostics for runtime/editor surfaces

### Ticket 20.4 - Structural Refresh Integration

Tie asset iteration more directly into scene/runtime rebuild behavior where necessary.

Deliverables:

* clearer connection to current-scene rebuild paths
* asset-driven rebuild suggestions or actions where appropriate
* safer handling of structural asset changes

### Ticket 20.5 - Expanded Regression Coverage

Add or expand tests covering:

* dependency-driven invalidation
* selected runtime action by asset type/policy
* richer explanation surfaces
* structural refresh and rebuild reasoning

---

## Required Minimum Slice

The minimum acceptable implementation for milestone success is:

1. stronger dependency reasoning for the currently supported major asset types
2. a clearer and more explainable refresh action model
3. better diagnostics describing why the engine chose reload, rebuild, or restart-oriented behavior
4. stronger integration with structural refresh and scene rebuild flow
5. tests proving the iteration model is more explicit and less ambiguous than before

If these land cleanly, the milestone succeeds even if:

* some later asset kinds still need deeper dependency modeling
* full dependency graphs or broader tooling UX are deferred

---

## Closeout Criteria

Milestone 20 is complete when:

* asset iteration decisions are easier to explain
* change detection and refresh behavior feel more intentional
* structural asset refresh is integrated with scene rebuild truth
* diagnostics are richer and more trustworthy
* the engine's iteration model is narrower where needed, but clearer everywhere

Completion does not mean "perfect hot reload."
It does mean Carrot's iteration architecture is becoming a durable engine strength instead of a set of useful but isolated mechanisms.
