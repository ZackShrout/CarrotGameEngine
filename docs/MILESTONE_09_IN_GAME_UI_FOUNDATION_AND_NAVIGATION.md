# Carrot Game Engine - Milestone 09

**Last Updated:** April 7, 2026  
**Title:** In-Game UI Foundation and Navigation  
**Status:** Planned  
**Focus:** Introduce a real engine-owned UI framework that is code-first, composable, and naturally navigable by keyboard and controller, without over-engineering or editor-first assumptions.

---

## Milestone Goal

Carrot now has:

- a structured render pipeline with a dedicated UI stage
- strong input abstraction through action maps
- controller support across platforms
- authored world and gameplay foundations
- a multi-window-capable runtime direction

However, it still lacks a real solution for **in-game user interface**.

Right now:

- debug text exists, but is not a UI system
- there is no engine-owned concept of UI widgets or layout
- there is no focus or navigation model for menus
- gameplay cannot yet present structured UI (menus, settings, inventory, dialogue, etc.)

Milestone 09 addresses that gap.

This milestone introduces a **UI foundation**, not a full UI ecosystem.

---

## Scope Summary

Milestone 09 is **not**:

- an editor UI system
- a visual UI authoring tool
- a full styling/skin system
- a complete widget library
- a data-binding or reactive framework
- a “clone Unity/Unreal UI” effort

This milestone **is**:

- a foundational UI runtime system
- a retained widget tree with clear ownership
- a layout system sufficient for real game UI
- a navigation system designed for keyboard/controller first
- a composable widget model suitable for reusable game-defined UI
- a clean integration into Carrot’s render pipeline and input model

---

## Why This Milestone Comes Next

Carrot’s current architecture has removed several major ceilings:

- render pipeline structure no longer blocks UI integration
- action-based input allows UI to be device-agnostic
- controller support establishes non-mouse interaction as first-class
- window system work prepares for future tooling/editor directions

The next structural limitation is not rendering or gameplay—it is **presentation and interaction**.

Without UI:

- menus cannot be built cleanly
- input rebinding cannot be exposed to players
- gameplay systems cannot surface information or choices
- engine usability for real games remains incomplete

This milestone introduces the layer that makes Carrot **usable as a game platform**, not just a gameplay foundation.

---

## Architectural Direction

### 1. Engine-Owned UI System

Carrot will introduce a **UI runtime subsystem** (e.g., `ui_module_t` or equivalent), responsible for:

- owning UI trees
- managing focus and navigation
- routing UI-relevant input actions
- coordinating layout and paint passes
- bridging UI output into the renderer

This is a **subsystem-level feature**, not a collection of helper functions.

---

### 2. Retained Widget Tree

UI will be built around a **retained-mode widget hierarchy**.

Key properties:

- widgets exist as persistent objects
- parent/child relationships define structure
- state is owned by widgets
- layout and rendering operate on the tree

This enables:

- reusable custom widgets
- stable focus behavior
- clean composition
- future editor/tooling compatibility

---

### 3. Code-First Composition

UI must be easy to author in C++.

The system should support:

- straightforward widget construction in code
- composition of widgets into larger structures
- reusable game-defined widgets (e.g., `swag_button_t`)
- minimal boilerplate for common UI patterns

The engine must not depend on visual authoring tools.

---

### 4. Navigation-First Input Model

UI navigation is **keyboard/controller first**, not mouse-first.

The system should:

- consume semantic UI actions (navigate, accept, cancel)
- integrate directly with the existing action map system
- maintain explicit focus state
- support directional navigation (up/down/left/right)
- allow explicit navigation overrides where needed

Mouse interaction may be supported, but must not define the architecture.

---

### 5. Layout as a First-Class System

UI layout should be explicit and engine-owned.

Initial layout support should include:

- vertical and horizontal stacking
- padding and spacing
- alignment rules
- fixed and flexible sizing

The goal is **clarity and usefulness**, not completeness.

---

### 6. Separation of Meaning and Rendering

Widgets define:

- structure
- behavior
- interaction
- layout intent

Rendering is handled by:

- a UI paint pass
- a renderer bridge layer

Widgets should not directly depend on backend rendering APIs.

---

### 7. Lightweight Styling

UI should support basic styling:

- colors
- text appearance
- simple visual states (normal, focused, pressed, disabled)

This should remain intentionally lightweight in this milestone.

---

## Core Concepts Introduced

- `ui_module_t` (or equivalent system root)
- `ui_widget_t` base type
- widget tree ownership model
- layout pass (measure + arrange)
- paint pass (render intent emission)
- focus and navigation system
- semantic UI input actions
- basic style/theming hooks

---

## Initial Widget Set

The first-pass widget set should remain small but useful:

- `ui_panel_t`
- `ui_label_t`
- `ui_button_t`
- `ui_stack_t` (vertical/horizontal)
- `ui_spacer_t`

These should be sufficient to construct real menus.

---

## Reusable Custom Widgets

The system must support game-defined widgets such as:

- custom button types
- menu rows
- inventory slots
- dialogue options

These may be implemented via:

- inheritance from base widgets
- composition of smaller widgets

The goal is to make reusable UI patterns **natural and ergonomic**.

---

## Definition of Done (Milestone-Level)

Milestone 09 is successful when:

1. Carrot has an engine-owned UI system with a retained widget tree.
2. UI can be constructed entirely in code using engine-provided primitives.
3. A real in-game menu (e.g., pause menu) can be built using the system.
4. That menu is fully navigable using keyboard and controller.
5. Focus behavior is deterministic and stable across UI changes.
6. UI renders correctly through the dedicated UI render stage.
7. Game code can define and reuse a custom widget type cleanly.
8. The system integrates cleanly with the existing action-based input model.

---

## Out of Scope (Explicitly Not Included)

- visual UI editor or GUI layout tools
- full animation/tweening system
- advanced styling/skin system
- data binding frameworks
- scrolling/virtualized lists
- complex text layout (rich text, markup)
- docking, editor panels, or tooling UI
- full accessibility systems

---

## Suggested Ticket Breakdown

### Ticket 1 — UI System Core
- Introduce `ui_module_t`
- Implement widget tree ownership
- Basic update + traversal structure

### Ticket 2 — Layout Foundation
- Vertical and horizontal stack containers
- Padding and spacing
- Basic sizing rules

### Ticket 3 — Focus and Navigation
- Focus tracking
- Directional navigation
- Accept/cancel handling
- Integration with action system

### Ticket 4 — Core Widgets
- Panel, label, button, spacer
- Basic interaction and visual states

### Ticket 5 — Styling and Reuse
- Lightweight style support
- Demonstrate reusable custom widget (e.g., `swag_button_t`)

### Ticket 6 — Integration Proof
- Implement a pause menu
- Validate navigation, layout, rendering, and reuse

---

## Notes for Future Milestones

This milestone establishes the foundation for:

- input rebinding UI
- inventory and gameplay UI
- dialogue systems
- debug and developer tools
- eventual editor UI
- more advanced layout and styling systems

These are not necessarily the next steps, but future work should build on this foundation, not replace it.