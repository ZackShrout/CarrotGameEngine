# Carrot Game Engine - Milestone 07

**Last Updated:** April 5, 2026
**Title:** Controller Input Support
**Status:** Draft
**Focus:** Add a real engine-owned controller input path that flows through Carrot's existing action-based input model rather than creating a separate gameplay-only input system.

---

## Milestone Goal

Milestone 05 made authored worlds render correctly.

Milestone 06 made Tiled a much stronger authored-data workflow.

The next ceiling is input breadth.

Right now Carrot has:

* keyboard-driven action bindings
* semantic action names such as movement and interaction
* gameplay code that already consumes actions instead of raw key checks in most important paths

But it still lacks a real answer for controller-driven play.

Milestone 07 is about making controller input a first-class engine feature.

That should mean:

* controller input is discovered and normalized by the engine
* gameplay code can keep asking for actions and movement intent instead of device-specific events
* controller support does not become a parallel one-off path beside keyboard input

---

## Scope Summary

Milestone 07 is **not** just "wire one gamepad to player movement."

It is a **controller input and action-routing milestone**.

The intended near-term direction is:

* engine-owned device support first
* action-map integration second
* analog movement and practical gameplay parity third

That means this milestone should prioritize:

* controller connection and state handling
* normalized controller button and axis vocabulary
* action-map support for controller bindings
* analog stick movement with deadzone handling
* engine/runtime debugability for controller state and bindings

It should not sprawl into:

* full user-facing rebinding UI
* broad menu-navigation UX polish
* rumble / haptics unless it falls out naturally
* per-platform controller icon sets
* online-input abstraction or rollback-style systems

---

## Why This Milestone Comes Next

Carrot now has stronger authored worlds and a better content pipeline.

The next high-value engine gap is that input is still effectively keyboard-only in practice.

Controller support matters because:

* it broadens the practical playability of Carrot games
* it reinforces the action-based input model already taking shape
* it prepares later UI work to think in terms of multi-device input from the start

This milestone should not replace the existing input action path.

It should complete it.

---

## Current Implementation Baseline

Carrot already has:

* keyboard input events routed through platform windows
* an engine-owned input action map
* JSON-authored default keyboard action bindings
* gameplay code that already uses semantic action names in the sandbox

Current notable gaps:

* no controller discovery or connection state
* no normalized gamepad button/axis vocabulary
* no controller bindings in the action config path yet
* no analog movement intent path
* no controller-facing validation or debug inspection data

---

## Ticket 1 - Controller Device Foundation

**Priority:** P0
**Outcome:** Carrot can detect and track controller devices through an engine-owned abstraction.

### Why

Before action bindings or gameplay movement can work cleanly, the engine needs a stable device layer.

### Scope

Add first-pass controller support for:

* controller connection and disconnection tracking
* stable per-device identity for runtime use
* normalized engine-facing button and axis state
* practical platform integration for the currently supported desktop targets

### Acceptance Criteria

* The engine can detect at least one connected controller on supported platforms.
* Controller button and axis state can be queried through engine types rather than platform-specific APIs.
* The device layer is engine-owned and usable by future input systems.

---

## Ticket 2 - Action Map Integration

**Priority:** P0
**Outcome:** Controller bindings flow through the same semantic action path as keyboard bindings.

### Why

Carrot already has an action-based input direction.

Controller support should strengthen that path, not fork around it.

### Scope

Extend the input action system to support:

* controller button bindings
* controller axis-to-action support where useful
* authored default bindings in the input config format
* validation and fallback behavior for malformed bindings

### Acceptance Criteria

* A gameplay action can be triggered by keyboard or controller through the same action-map interface.
* The input config format can express controller bindings intentionally.
* Invalid controller bindings fail safely and clearly.

---

## Ticket 3 - Analog Movement and Practical Gameplay Support

**Priority:** P0
**Outcome:** Player movement and common gameplay actions work naturally on controller.

### Why

Digital button support alone is not enough for controller-first play.

Top-down and hybrid 2D games benefit heavily from analog movement feel.

### Scope

Add practical support for:

* left-stick movement intent
* deadzone handling
* normalized analog magnitude and direction
* sandbox player parity with current keyboard-driven movement and interaction

### Acceptance Criteria

* The player can move and interact comfortably on controller in the sandbox.
* Analog movement does not require gameplay code to read raw platform controller state.
* Deadzone behavior is engine-owned and documented.

---

## Ticket 4 - Debugability, Validation, and Authoring Guidance

**Priority:** P1
**Outcome:** Controller support is easier to trust and extend.

### Why

Input bugs are frustrating when the engine cannot explain what device or binding state it believes is active.

### Scope

Add practical support such as:

* engine-side controller debug snapshots or stats
* validation around controller-authored binding data
* docs for the updated input action config contract
* regression coverage for action-map controller behavior where practical

### Acceptance Criteria

* The engine exposes enough non-visual debug information to reason about controller state and bindings.
* Controller binding support is documented alongside the existing input action schema.
* Future controller-input work becomes easier to extend safely.

---

## Non-Goals

Milestone 07 should **not** expand into:

* a full UI navigation milestone
* advanced rebinding screens
* controller-specific menu art and prompts
* networked input systems
* platform certification edge cases beyond what is needed for a strong first pass

---

## Success Criteria

Milestone 07 is successful if:

* controller devices are recognized through an engine-owned abstraction
* controller bindings participate in the same action model as keyboard bindings
* sandbox movement and interaction feel meaningfully playable on controller
* the input contract is documented clearly enough for future UI and gameplay work to build on

---

## Likely First Concrete Targets

If this milestone starts immediately, the strongest first implementation targets are probably:

1. define the engine-facing controller device vocabulary
2. extend the action-map data model to express controller bindings
3. wire controller input into sandbox movement and interaction through semantic actions
4. add docs, validation, and engine-side debug snapshots alongside the feature

That keeps the milestone focused on input architecture and playability instead of turning into a general UX or UI milestone.
