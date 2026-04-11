# Carrot Game Engine - Milestone 11

**Last Updated:** April 10, 2026
**Title:** Runtime UI Usability and Native MSDF Text Rendering
**Status:** Proposed
**Focus:** Turn Carrot's first-pass UI foundation into a genuinely usable engine-owned runtime UI layer, led by a Carrot-native MSDF text pipeline and stronger layout quality rather than sandbox-specific feature work.

---

## Milestone Goal

Milestone 09 established a real engine-owned UI foundation:

* retained widget tree
* layout primitives
* focus/navigation
* UI input ownership policy
* renderer-stage integration

That foundation is real, but it is not yet pleasant enough for serious game runtime use.

The current largest practical ceiling is **text quality**.

Right now Carrot can render text, but the current pre-baked one-size bitmap path is not strong enough for:

* clean menu presentation
* dialogue boxes
* settings screens
* HUD text
* scalable UI styling

This milestone is about fixing that ceiling in an **engine-first** way.

The proposed primary text direction for this milestone is:

* **Carrot-native MSDF text rendering**
* **Carrot-owned cooked font assets**
* **portable vertex/fragment shader baseline across Vulkan, Metal, and DirectX 12**

Geometry-shader experimentation can still be explored separately where supported, but it must **not** become the portability baseline for Carrot text.

The sandbox should act as the proof target, but the work itself should primarily strengthen engine systems.

---

## Scope Summary

Milestone 11 is **not**:

* a sandbox UI content milestone
* an editor tooling milestone
* a full skin/theme editor
* a giant widget-catalog milestone
* a localization/content-pipeline overhaul

Milestone 11 **is**:

* a Carrot-native MSDF text milestone
* a native cooked font asset milestone
* a text layout and measurement milestone
* a runtime UI rendering usability milestone
* a small engine-owned widget expansion milestone
* a proof that real in-game UI can look acceptable in Carrot

---

## Milestone Relationship

Milestone 11 owns the first real runtime-text consumer case for cooked font artifacts.

That means:

* Milestone 11 should define and deliver the first `.cfont` path needed for native MSDF text
* Milestone 11 does **not** need to solve the entire generalized imported-artifact architecture for every asset type
* Milestone 12 should later generalize and harden the broader imported/cooked asset model around the same architectural rules

This keeps milestone ownership clean:

* Milestone 11 proves the font/text/UI slice
* Milestone 12 broadens the imported-artifact system across asset classes

---

## Why This Milestone Comes Next

The engine already has:

* explicit frame stages for world, UI, composite, debug, and log surfaces
* a retained UI framework
* action-driven navigation
* scene/runtime foundations
* multi-window runtime support

The next major ceiling is presentation quality.

Without stronger text rendering:

* any pause menu will look rough
* dialogue systems will feel prototype-grade
* settings/inventory screens will be harder to trust as real engine UI
* future styling work will sit on top of a weak primitive

This milestone improves the primitive that most visibly controls UI quality.

It also establishes a text path that fits Carrot's engine values:

* native and explicit
* renderer-owned
* compatible with the existing asset/import architecture
* portable across Carrot's graphics backends without requiring geometry shaders

---

## Definition of Done (Milestone-Level)

Milestone 11 is successful when all of the following are true:

1. Carrot has a native MSDF text rendering path that supports multiple practical UI sizes cleanly.
2. UI code can measure, align, and wrap text through engine APIs rather than ad-hoc sandbox helpers.
3. Carrot has a native cooked font asset path suitable for runtime UI text rendering.
4. The engine exposes enough widget/rendering primitives to build a real pause/dialogue/settings proof without custom sandbox rendering hacks.
5. The sandbox demonstrates the new UI path as proof, but the bulk of the implementation remains in engine systems.
6. The renderer/UI architecture remains clean and does not collapse text handling into game-specific code.

---

## Ticket 1 - Carrot-Native MSDF Text Pipeline

**Priority:** P0
**Outcome:** Carrot has a native MSDF-based text rendering path suitable for runtime UI.

### Why

The current one-size pre-baked bitmap approach is visibly holding back UI quality.
MSDF is the chosen direction because it provides crisp scaling while fitting Carrot's explicit renderer/asset model well.

### Scope

Design and implement a Carrot-owned MSDF text path with:

* MSDF atlas sampling in the renderer
* glyph metrics and baseline-aware placement
* colorized runtime UI text
* portable vertex/fragment shader support across supported graphics backends

This milestone should not require geometry shaders.
Optional geometry-shader experiments may happen later, but they are not part of the required architecture.

### Acceptance Criteria

* Text quality is visibly improved over the current path in real runtime UI.
* The engine can render the same text content at multiple UI sizes without obviously bad scaling artifacts.
* The baseline implementation works through Carrot's normal cross-backend renderer model.
* Text rendering remains renderer/UI owned, not sandbox owned.

---

## Ticket 2 - Native Cooked Font Asset and Import Path

**Priority:** P0
**Outcome:** Carrot has an engine-owned cooked font asset suitable for MSDF runtime text rendering.

### Why

MSDF rendering wants more than a runtime shader tweak.
It wants a real asset format carrying atlas data and glyph metrics.

### Scope

Design and implement a native cooked font path, likely along the lines of:

* a Carrot-owned cooked font asset format such as `.cfont`
* native font import/generation code owned by Carrot
* atlas, glyph metric, and layout metadata output required by the runtime text path

The goal is to keep the implementation Carrot-native rather than leaning on a third-party runtime text framework.

### Acceptance Criteria

* Carrot has a native cooked font asset format suitable for MSDF text rendering.
* Runtime UI text no longer depends on the old pre-baked single-size path.
* The import/output model fits Carrot's broader asset pipeline direction cleanly.

### First-Pass `.cfont` Addendum

The first-pass `.cfont` artifact should stay intentionally narrow and practical.

Recommended first-pass contents:

* cooked-format version
* importer/generator version
* source font identity/fingerprint data needed for invalidation
* atlas dimensions and atlas texture format
* glyph table keyed by codepoint
* per-glyph atlas bounds/UVs
* per-glyph advance
* per-glyph plane bounds or equivalent placement metrics
* line height, ascent, descent, and baseline-related metrics needed for layout
* kerning pairs or equivalent first-pass pair-adjustment data if supported in this milestone

Important first-pass rule:

* `.cfont` should contain the runtime-facing data needed to render and lay out text
* it should not try to become a universal editable font project format
* it should be optimized for Carrot runtime/UI needs first

---

## Ticket 3 - Introduce Engine-Owned Text Layout Primitives

**Priority:** P0
**Outcome:** UI systems can measure and arrange text as a first-class engine capability.

### Why

Good rendering alone is not enough. Runtime UI needs reliable text layout behavior.

### Scope

Add engine APIs for:

* text measurement
* line height/baseline access where practical
* wrapping within a width constraint
* horizontal alignment
* vertical placement rules suitable for labels and dialogue text

### Acceptance Criteria

* Labels and text containers can size themselves from measured text.
* Basic wrapped text can be presented without sandbox-specific layout logic.
* UI layout code can reason about text dimensions deterministically.

---

## Ticket 4 - Strengthen Core Runtime UI Widgets Around Text

**Priority:** P1
**Outcome:** The UI layer becomes materially more usable without sprawling into a giant widget library.

### Why

Once text is stronger, the next leverage is a small number of engine-owned widgets that prove runtime usefulness.

### Scope

Improve or add the minimum set of engine widgets needed for real runtime UI proof, likely including:

* better `ui_label_t` or equivalent text widget
* button text integration improvements
* simple text container/panel composition support
* straightforward styling/state hooks for focused/disabled/selected text presentation

### Acceptance Criteria

* The engine can assemble readable menus and dialogue-style panels without custom one-off draw code.
* Widget behavior remains composable and code-first.
* The sandbox proof primarily consumes engine widgets instead of inventing new game-owned rendering paths.

---

## Ticket 5 - Sandbox Proof: Real Runtime UI, Minimal Game-Specific Logic

**Priority:** P1
**Outcome:** The milestone is validated by a practical runtime proof without becoming a sandbox-heavy feature spree.

### Why

The sandbox should prove the engine systems are usable, not become the main implementation site.

### Scope

Build a small, reviewable proof set such as:

* pause menu
* dialogue/sign presentation box
* simple settings/debug panel

Use this proof to validate:

* text quality
* focus/navigation
* wrapping/alignment
* layout behavior
* UI input ownership

### Acceptance Criteria

* The sandbox demonstrates at least two genuinely readable runtime UI surfaces.
* The proof uses engine widgets, engine text, and engine layout.
* The sandbox does not become the primary home for UI architecture.

---

## Ticket 6 - Regression Coverage for Text and UI Presentation

**Priority:** P1
**Outcome:** Text and UI quality gains are protected by automated checks where practical.

### Why

The UI stack is now deep enough that regression protection matters.

### Scope

Expand tests around:

* text measurement invariants
* wrapping behavior
* label/widget sizing behavior
* UI navigation and ownership interactions with text-heavy widgets

Keep the tests deterministic and engine-owned.

### Acceptance Criteria

* New text/layout functionality is covered by focused tests.
* Existing UI tests remain green with the stronger text path.
* Test additions validate engine behavior, not sandbox-specific cosmetic choices.

---

## Out of Scope by Design

The following should stay out of Milestone 11 unless a later review explicitly pulls them in:

* localization pipeline design
* rich text/markup system
* editable text fields
* full skin/theme authoring framework
* animation-heavy menu presentation systems
* broad inventory/journal/gameplay UI feature design
* geometry shaders as a required part of Carrot's text architecture
* reliance on a third-party runtime text rendering framework

---

## Review Questions

These are the main review questions this milestone document is intended to settle before implementation:

1. What should the first-pass native `.cfont` artifact contain?
2. How much text layout belongs in the engine now versus later?
3. What is the minimum runtime UI proof set that validates the engine work without drifting into sandbox-first design?
4. Which widget primitives are truly required in this milestone, and which should stay deferred?

---

## Current Recommendation

If implementation started directly from this draft, the recommended first pass would be:

1. Define `.cfont` and the native font import/generation path.
2. Add the renderer-side MSDF shader and atlas sampling path.
3. Add text measurement/layout APIs.
4. Strengthen label/button text rendering through engine widgets.
5. Validate with a small sandbox pause/dialogue/settings proof.
