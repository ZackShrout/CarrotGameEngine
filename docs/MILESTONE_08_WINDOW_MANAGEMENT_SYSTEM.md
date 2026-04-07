# Carrot Game Engine - Milestone 08

**Last Updated:** April 7, 2026
**Title:** Window Management System
**Status:** Planned
**Focus:** Replace one-window bring-up assumptions with an engine-owned multi-window foundation that is ready for future UI/tooling/editor work.

---

## Milestone Goal

Milestone 07 completed controller input support.

The next major architectural ceiling is windowing and presentation ownership.

Right now Carrot still has strong single-window assumptions in key paths:

* global primary-window accessors
* renderer startup tied to one implicit window size
* input/resize binding wired only for one main window
* engine loop behavior centered on a single presentation target

Milestone 08 is about removing that ceiling before in-game UI and tooling deepen it.

This milestone should produce a real engine window management system, not just "open one extra test window."

---

## Scope Summary

Milestone 08 is **not** an editor UX milestone.

It is a **window lifecycle, routing, and presentation foundation milestone**.

The intended direction is:

* explicit engine-owned window management first
* per-window event and presentation ownership second
* practical multi-window proof third

That means this milestone should prioritize:

* stable window IDs and lifecycle management
* per-window event routing (close, resize, focus, input)
* render/presentation decoupling from global primary-window assumptions
* deterministic multi-window main-loop behavior
* a small read-only log window as a system proof

It should not sprawl into:

* docked editor panels or saved workspace layouts
* command consoles, filtering UIs, or log search tools
* full multi-viewport editor interaction design
* broad UI toolkit design or widget architecture
* game feature work unrelated to windowing foundations

---

## Why This Milestone Comes Next

Carrot now has stronger gameplay, authored-world, and input foundations.

The current risk is architectural lock-in: if UI and tooling work continue on top of one-window assumptions, later multi-window/editor work will become much more expensive.

Milestone 08 is the right moment to:

* convert windowing from bring-up code into an engine system
* protect upcoming UI work from one-window coupling
* prove the engine can run multiple windows in a controlled way

---

## Definition of Done (Milestone-Level)

Milestone 08 is successful when all of the following are true:

1. The engine can create and manage multiple top-level windows through an engine-owned window manager.
2. Two separate windows can render the sandbox world simultaneously.
3. A third smaller window can render the full engine log stream with per-severity color styling.
4. Window close, resize, and focus behavior remain stable and deterministic across all active windows.
5. Logging fan-out to console plus window sink works without breaking existing log behavior.
6. The implementation remains foundation-focused and does not expand into full editor/log-tool UX.

---

## Ticket 1 - Engine Window Manager Core

**Priority:** P0  
**Outcome:** Carrot has an engine-owned window manager with explicit multi-window lifecycle ownership.

### Why

Current window code is built around a primary-window singleton model.

That is useful for bring-up, but it is not a long-term engine system.

### Scope

Introduce a `window_manager_t`-style engine system that supports:

* create/destroy window operations
* stable `window_id` identity
* lookup/enumeration of active windows
* explicit "main gameplay window" designation as policy, not architecture

### Acceptance Criteria

* Engine runtime can own more than one window object at once.
* Window creation/destruction no longer depends on global primary-window state.
* The engine can iterate active windows safely each frame.

---

## Ticket 2 - Per-Window Event Routing and Input Ownership

**Priority:** P0  
**Outcome:** Window events and input are routed through explicit per-window ownership paths.

### Why

Single-window event bindings are currently hardwired and do not scale to tools or auxiliary windows.

### Scope

Add per-window routing for:

* close requests
* resize notifications
* focus/minimize state
* keyboard/mouse event dispatch

Define and implement first-pass focus/input policy:

* one designated gameplay-input target window
* non-gameplay windows (for now) consume no gameplay input by default

### Acceptance Criteria

* Each active window receives its own event callbacks and state updates.
* Close/focus behavior is deterministic when multiple windows are open.
* Gameplay input remains stable and predictable with multiple windows present.

---

## Ticket 3 - Rendering and Presentation Decoupling

**Priority:** P0  
**Outcome:** Rendering is no longer tied to global primary-window getters.

### Why

The current renderer startup and resize flow still assumes one implicit window.

### Scope

Refactor presentation ownership so that:

* per-window presentation targets/surfaces are explicit
* renderer-facing flow uses window-specific size and state
* resize/minimize handling is per-window and safe

The goal is foundation readiness, not full multi-viewport editor rendering.

### Acceptance Criteria

* Renderer no longer depends on global singleton window dimensions for core flow.
* Per-window presentation works for at least two sandbox-rendering windows.
* Minimize/resize behavior in one window does not corrupt the other window's presentation path.

---

## Ticket 4 - Multi-Window Sandbox Proof

**Priority:** P0  
**Outcome:** The milestone has a concrete runtime proof that multi-window foundations are real.

### Why

Architectural claims need an end-to-end validation path.

### Scope

Add a deterministic startup mode that creates:

* primary sandbox window
* secondary sandbox-rendering window

Define practical policy for this milestone:

* both windows render the same sandbox world
* gameplay input goes to designated main gameplay window only
* camera behavior can be shared for first pass unless explicit divergence is needed

### Acceptance Criteria

* Both sandbox windows render successfully during normal runtime.
* Both can be resized/moved/focused without destabilizing the engine loop.
* Closing one non-primary window does not crash or force unintended full-engine shutdown.

---

## Ticket 5 - Log Window Sink and Read-Only Log Console Window

**Priority:** P1  
**Outcome:** A small third window displays live colorized engine logs as a foundational diagnostics proof.

### Why

A log window is a strong multi-window systems proof without requiring full tooling UX.

### Scope

Implement:

* a window log sink that receives all `logger_t` messages
* bounded in-memory buffering for log entries
* a small read-only log window (target position: bottom-left of monitor, practical fixed default size)
* per-severity color rendering aligned with existing log severity styling

Keep this intentionally minimal:

* no filtering/search
* no command input
* no advanced console UX

### Acceptance Criteria

* All log categories currently emitted by the engine appear in the log window.
* Severity levels are visually distinct and stable.
* Existing console logging remains intact (fan-out, not replacement).
* Log rendering remains bounded and does not grow unbounded memory usage.

---

## Ticket 6 - Validation, Docs, and Guardrails

**Priority:** P1  
**Outcome:** The new window system is easier to trust, maintain, and extend in later milestones.

### Why

Windowing regressions can be subtle. This milestone needs explicit guardrails.

### Scope

Add:

* focused regression coverage for multi-window lifecycle and routing behavior where practical
* clear internal docs for window manager responsibilities and ownership boundaries
* documented follow-on constraints for UI/tooling milestones

### Acceptance Criteria

* Milestone docs and architecture notes clearly describe the new window model.
* Basic lifecycle and routing regressions are covered by tests or deterministic runtime checks.
* The milestone closeout calls out what intentionally remains out of scope.

---

## Non-Goals

Milestone 08 should **not** expand into:

* full in-game UI system design
* full editor shell/docking/workspace systems
* rich interactive log tooling
* generalized window-layout persistence
* solving every future tooling workflow

---

## Risks and Mitigations

**Risk:** Milestone slips into editor UX implementation.  
**Mitigation:** Enforce strict non-goals and keep the log window read-only.

**Risk:** Multi-window render path creates unstable input/focus behavior.  
**Mitigation:** Explicitly lock gameplay input ownership to one designated window for Milestone 08.

**Risk:** Runtime complexity grows without enough testability.  
**Mitigation:** Add deterministic startup proof mode and targeted lifecycle/routing checks.

---

## Suggested Implementation Order

1. Window manager core and window identity/lifecycle.
2. Per-window event routing and gameplay-input ownership policy.
3. Renderer/presentation decoupling from global primary-window assumptions.
4. Two-window sandbox rendering proof.
5. Log sink plus third read-only log window.
6. Validation/docs/closeout.

This order keeps the hard architecture seams first and the proof layers on top.
