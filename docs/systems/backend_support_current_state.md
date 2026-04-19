# Carrot Backend Support - Current State

**BunnySoft**
**Current backend support note**
**Last Updated: April 18, 2026**

---

## 1. Purpose

This note records what Carrot currently means when it says backend support or backend parity.

It is intentionally narrower than "everything the RHI could theoretically expose."
The goal is to describe the current renderer-facing truth honestly.

See also:

* [rhi_contract_current_slice.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/rhi_contract_current_slice.md:1) for the lower-level contract audit
* [backend_parity_current_slice_checklist.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/backend_parity_current_slice_checklist.md:1) for the checklist used to keep this support claim honest

---

## 2. Supported Graphics Backends

Carrot currently targets three native graphics backends:

* Vulkan
* Metal
* DirectX 12

All three participate in the current practical renderer contract through `rhi_context_t`.

That current slice includes:

* world textured-quad rendering
* world text rendering
* UI stage rendering
* composite stage rendering
* overlay debug stage rendering
* log console stage rendering
* context-level texture, buffer, and sampler creation
* resize handling
* auxiliary presentation-window support where the backend currently implements it

---

## 3. What "Practical Parity" Means Right Now

Carrot currently uses "practical parity" to mean:

* the renderer uses the same context-level contract across Vulkan, Metal, and DirectX 12
* the supported renderer stages stay aligned in space, presentation routing, and lighting expectations
* shared shader / renderer growth is expected to preserve that slice

It does **not** mean:

* every backend exposes identical low-level helper methods
* every older abstraction surface remains equally important
* every possible future renderer feature is already parity-hardened

It also does not yet mean:

* compute/state handoff is already a broad arbitrary-pass feature
* pass-boundary synchronization expectations for compute-driven work are already implemented end to end

The current parity story is therefore:

* honest for the current renderer slice
* intentionally narrower than a blanket claim of complete backend sameness

---

## 4. Current Validation Shape

Carrot currently validates backend parity through a mix of automated and manual checks.

Automated support currently protects the shared renderer contract through:

* null-backend stage recording regressions
* null-backend compute and indirect contract regressions
* presentation-channel and auxiliary-window delegation tests
* shared limit validation around stage slots, presentation routing, and forward+ limits

Manual validation still matters for native output:

* Vulkan should be exercised against the current Sandbox slice
* Metal should be exercised against the current Sandbox slice
* DirectX 12 should be exercised on Windows against the same slice

At the time of this note:

* On **April 18-19, 2026**, Vulkan was manually exercised against the milestone 23 compute-capable slice and exited clean after the compute-buffer lifetime fix
* On **April 18-19, 2026**, Metal was manually exercised against the same slice and ran clean after the auxiliary-window scissor validation fix
* On **April 19, 2026**, DirectX 12 successfully compiled the milestone 23 codepath, but native runtime validation remained pending because the Windows environment still needed cleanup after dependency/runtime-linker issues

---

## 5. Important Current Limitations

Current limitations and cautions include:

* parity claims are attached to the current context-level renderer slice, not every historical RHI surface
* backend-local helper methods are allowed where useful, but they are not shared parity promises by default
* future renderer work still needs discipline so new features do not quietly become one-backend-first
* native backend pixel-output validation is still partly manual outside the null-backend regression harness
* the compute-capable contract described in milestone 23 is now live through shared compute pipeline creation, dispatch, expanded buffer usages, explicit compute-to-graphics handoff declaration, and one narrow indirect draw path, but resource-state rules are still intentionally narrow
* the currently supported synchronization slice is explicit compute-before-graphics handoff for storage data, not a general arbitrary-pass synchronization model
* the currently supported indirect slice is one context-level textured-quad indexed-indirect path, not a broad generic indirect-command surface
* the current milestone 24 slice now uses GPU compute for live world forward+ tile/light classification on Vulkan and Metal, while DirectX 12 native runtime validation still needs the Windows follow-up pass; milestone 24 should be treated as implemented-but-not-yet-archived until that validation lands
* renderer debug stats still report world-light overflow and forward+ tile/light-reference counts after the move to GPU classification; the current dropped-reference stat is expected to stay at zero under today's shared caps because the total world-light limit matches the per-tile light-index budget
* for Metal shader-converter pipelines, offline reflection JSON should be treated as a layout hint rather than a final authority for encoder bind indices; actual bind points still need confirmation against working backend patterns, validation output, and runtime behavior

---

## 6. Engineering Expectation

New renderer or RHI work should preserve this rule:

**If a feature is treated as part of the current supported renderer slice, parity expectations should be written down and validated as part of the work.**

That means:

* grow the renderer through the context-level contract unless a lower-level exception is clearly justified
* keep shared limits explicit
* document when support is current, partial, or pending platform-specific validation
