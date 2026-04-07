# Carrot Game Engine - Milestone 07

**Last Updated:** April 7, 2026
**Title:** Controller Input Support
**Status:** Complete
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

## Closeout Summary (April 7, 2026)

Milestone 07 is now considered complete.

Delivered outcomes:

* engine-owned controller discovery/state path on Windows, Linux, and macOS
* shared semantic action routing for keyboard plus controller bindings
* analog movement support through shared gameplay-facing movement intent
* sandbox gameplay parity for movement and interaction
* runtime debug visibility for connected count, active slot, raw/stable state
* regression coverage for action-map/controller/player-controller integration
* backend-aware stabilization policy without preprocessor branching:
  * Windows/Linux keep release debounce enabled
  * macOS runs with debounce disabled based on stable hardware validation

Deferred follow-up (post-milestone hardening, not milestone blockers):

* broader controller-family validation (including PlayStation-style hardware when available)
* additional per-platform quirk notes as more devices are tested
* future stabilization re-tuning only if broader hardware data indicates it

---

## Current Implementation Baseline

Milestone 07 now has real first-pass implementations on all desktop targets.

What is currently in place:

* an engine-owned controller manager and normalized controller state vocabulary
* platform-separated controller backends selected through CMake
* a Windows backend using GameInput
* a Linux backend using `libudev` plus `libevdev`
* a macOS backend using Apple's GameController framework
* controller button and axis-threshold support in the input action map
* JSON-authored default controller bindings in the sandbox input config
* sandbox movement parity across keyboard, d-pad, and left stick
* sandbox interaction parity across keyboard and controller face button
* controller debug overlay data showing connected count, active slot, raw state, and stabilized state
* regression coverage for controller/action-map/player-controller integration

Current notable gaps:

* no broad controller-family validation yet across all desktop backends
* no PlayStation-style controller validation data yet
* per-platform quirks still need to be expanded as more hardware is tested

---

## Current Status Snapshot

Milestone 07 is no longer a design-only milestone.

The current state is:

* Windows controller bring-up is working in the sandbox
* Linux controller bring-up is working in the sandbox
* macOS first-pass controller bring-up is now wired through GameController
* the Windows backend is on GameInput rather than XInput
* the Linux backend uses `libudev` for discovery/hotplug and `libevdev` for polling/normalization
* the macOS backend currently polls connected `GCController` instances and maps them into the shared normalized gamepad vocabulary
* the action map now supports keyboard plus controller bindings through one shared semantic action path
* player movement resolves per tick from keyboard, d-pad, and left stick
* controller state debug visibility exists in-engine and was used to validate a raw-input flutter issue on Windows
* Linux sandbox validation confirmed movement, interaction, buttons, and sticks behave correctly with the shared action/gameplay path
* a time-based engine stabilization layer currently smooths short raw-release flutter without changing the shared gameplay-facing API

Most important conclusion so far:

* the raw button flutter observed during hold testing reproduced in the raw backend state even after moving from XInput to GameInput
* that means the current stabilization layer is compensating for noisy raw controller state on the tested Windows setup rather than masking an engine action-map bug
* the current stabilization policy is intentionally time-based and device-agnostic, not a frame-count hack

---

## Post-Milestone Follow-Ups

The strongest next step after Milestone 07 is broader hardware validation across controller families.

That follow-up should focus on:

* broadening validation on macOS controller hardware and transport types (wired, Bluetooth, first-party and third-party)
* broadening hardware validation across more controller types on Windows and Linux
* comparing Linux raw-vs-stable behavior against Windows before changing the shared stabilization policy
* deciding whether any backend-specific normalization or deadzone policy should move out of gameplay-side assumptions

Recommended follow-up order:

1. run broader hardware validation across macOS, Windows, and Linux devices beyond the initial successful sandbox pass
2. compare Windows and Linux raw-vs-stable behavior before changing the shared stabilization policy
3. compare macOS raw-vs-stable behavior against Windows/Linux before changing shared stabilization
4. document any controller quirks or package requirements that surfaced during platform bring-up
5. revisit whether any additional regression coverage is warranted for backend-side normalization helpers

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

---

## Agreed Implementation Direction

The current intended implementation direction for Milestone 07 is:

* keep platform-specific controller code as small as possible
* let shared engine systems own controller policy, normalization, routing, and gameplay-facing behavior
* keep validating the shared engine/controller path across broader hardware on every desktop backend
* treat new controller-family behavior as data-driven follow-up rather than changing the shared API shape

This matches Carrot's broader architecture direction:

* platform code should translate native input into a stable engine-facing contract
* the engine should own deadzones, action routing, active-device policy, and movement-intent behavior
* gameplay code should keep consuming semantic actions and engine-facing movement intent rather than platform APIs

### Platform Validation Expectations

Current practical expectation after milestone closeout:

* **Windows:** implemented and sandbox-validated
* **Linux:** implemented and sandbox-validated
* **macOS:** implemented and sandbox-validated on Xbox-style hardware (8BitDo SN30 Pro), with debounce disabled by backend policy

Important rule:

* an unverified platform backend should fail soft and clearly
* unexpected controller enumeration or state issues should leave the engine in a safe "no active controller" state rather than poisoning input behavior

---

## Concrete Proposal

Milestone 07 should be approached as four connected layers of work:

1. platform/controller device foundation
2. shared engine controller system
3. action-map and gameplay-facing integration
4. debug, validation, docs, and tests

### 1. Platform / Device Foundation

Platform code should only be responsible for:

* detecting connected controllers
* reading native button/axis state
* surfacing connection and disconnection changes
* mapping native controller concepts into Carrot's normalized controller vocabulary

Platform code should **not** own:

* deadzone policy
* gameplay movement behavior
* action routing policy
* sandbox-specific decisions

This milestone should initially support:

* one active gameplay controller
* internal structures that are multi-controller-aware enough to grow later without a rewrite

Recommended active-controller policy for the first pass:

* first connected controller becomes the active gameplay controller
* later milestones can revisit more advanced "most recently used" behavior if needed

### 2. Shared Engine Controller System

The engine should own a controller/input service that:

* updates controller state each frame
* tracks connected devices
* chooses and exposes the active gameplay controller
* normalizes controller button and axis state
* applies deadzone behavior in shared code
* exposes engine-facing controller queries to future systems
* logs controller connection / disconnection / active-device changes

This should be a **state-based** system, not a gameplay-only callback path.

Controllers behave more naturally as sampled devices than as purely event-driven inputs.

Recommended normalized controller vocabulary:

* face buttons
* d-pad directions
* shoulder buttons
* stick clicks
* start / back style buttons
* left/right stick axes
* left/right trigger axes

### 3. Action Map and Gameplay Integration

Controller support should strengthen the existing action-based input path rather than replacing it.

#### Action Map Scope

The action map should be extended to support:

* keyboard key bindings
* controller button bindings
* optional controller axis-threshold bindings where useful for discrete actions

The action map should remain focused on **discrete semantic actions**.

That means it is a good fit for:

* interaction
* menu confirm/cancel later
* debug toggles
* quit-style actions where intentionally configured
* d-pad-as-discrete-action bindings

It is **not** expected to become a fully general analog-routing system during this milestone.

#### Movement Intent Direction

Analog movement should have an explicit gameplay-facing movement-intent path.

The intended direction is:

* keyboard contributes a digital movement vector through semantic actions
* d-pad contributes a digital movement vector through semantic actions and movement support
* left stick contributes an analog movement vector
* shared engine/game-facing logic resolves those inputs into one movement intent each tick

Recommended first-pass resolution policy:

* if left-stick magnitude is meaningfully outside the deadzone, use left-stick movement intent
* otherwise fall back to the combined digital movement vector from keyboard and d-pad

This preserves controller-first analog movement while still making d-pad and keyboard movement work naturally.

#### D-pad Decision

For this milestone, the d-pad should do both:

* participate in discrete action bindings
* contribute to gameplay movement as a digital movement source

This is intentionally included now rather than postponed, because it is part of the practical expected end state for controller support.

#### Player Controller Direction

The current `player_controller_t` already has a strong movement core, but its public input shape is still largely boolean-driven.

The intended direction is:

* preserve current boolean movement input support during transition
* add a movement-vector / movement-intent path
* update sandbox movement resolution so it happens per tick rather than only when keyboard events arrive

This allows controller analog movement to fit naturally without forcing gameplay code to read raw controller state.

### 4. Debug, Validation, Docs, and Tests

For this milestone, debugability is now expected to include both log output and a lightweight on-screen engine debug overlay.

Current debug expectations:

* log controller connected / disconnected events
* log active controller changes
* log controller-binding validation failures clearly
* avoid noisy per-frame controller-state spam by default
* expose enough live on-screen controller state to compare raw and stabilized behavior during bring-up

Docs and tests should cover:

* the updated input-action config contract
* controller binding validation behavior
* action-map controller matching behavior
* safe fallback behavior when controller bindings are malformed
* stabilization/debug expectations clearly enough that future backend work can be verified quickly

---

## Agreed Sandbox Defaults

Current agreed sandbox policy for Milestone 07:

* movement should support keyboard, d-pad, and left stick
* interaction should support keyboard plus a controller face button
* fullscreen toggle remains keyboard-only
* controller support should not imply that all sandbox/system actions automatically receive controller defaults

This matters because some bindings are engine-capable but still belong to game or sandbox policy.

For example:

* the engine should support controller-bound actions
* the sandbox does **not** currently want fullscreen toggling bound to a controller by default

---

## Input Config Direction

The preferred JSON direction for milestone 07 is to stay explicit and readable rather than introducing a broad generalized device-binding schema immediately.

Preferred examples:

```json
{ "action": "interact", "key": "E" }
{ "action": "interact", "gamepad_button": "South" }
{ "action": "move_left", "gamepad_button": "DPadLeft" }
```

If discrete axis-threshold bindings are added during milestone 07, they should also remain explicit.

Example:

```json
{ "action": "move_left", "gamepad_axis": "LeftX", "direction": "Negative", "threshold": 0.5 }
```

Why this direction is preferred:

* it keeps the schema easy to read
* it keeps validation straightforward
* it matches the current intentionally explicit style of the existing input binding format
* it avoids prematurely over-generalizing the action config model

---

## Recommended Implementation Order

After Milestone 07 closeout, the recommended order is:

1. run broader hardware validation across Windows, Linux, and macOS controller families
2. expand per-platform controller quirk documentation as data emerges
3. add any targeted regression coverage motivated by hardware findings
4. re-evaluate debounce defaults only if broader validation indicates a need
5. defer UI rebinding/navigation work to its own milestone

What is already complete enough to build on:

* normalized controller engine types and shared controller manager
* Windows backend bring-up
* action-map/config integration for controller bindings
* per-tick movement-intent resolution
* sandbox movement and interaction wiring
* first-pass docs, validation, and engine debug snapshots
