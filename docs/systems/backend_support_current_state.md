# Carrot Backend Support - Current State

**BunnySoft**
**Current backend support note**
**Last Updated: April 20, 2026**

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

* On **April 19, 2026**, Vulkan was manually exercised against the current milestone 25 world-stage GPU-driven slice and rendered correctly
* On **April 19, 2026**, Metal was manually exercised against the same slice and rendered correctly
* On **April 19, 2026**, DirectX 12 was manually exercised on Windows against the same slice and rendered correctly
* On **April 20, 2026**, Vulkan was manually exercised against the current milestone 26 composite/transition slice, including battle swirl and auxiliary presentation behavior, and rendered correctly
* On **April 20, 2026**, Metal was manually exercised against the same milestone 26 slice, including battle swirl and auxiliary presentation behavior, and rendered correctly
* On **April 20, 2026**, DirectX 12 was manually exercised on Windows against the same milestone 26 slice and rendered correctly

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
* the current milestone 24 slice now uses GPU compute for live world forward+ tile/light classification on Vulkan, Metal, and DirectX 12; the DirectX 12 runtime validation pass also clarified that `cpu_writable` storage uploads are a logical contract and may flow through backend-local staging before reaching the final GPU-visible storage resource
* renderer debug stats still report world-light overflow and forward+ tile/light-reference counts after the move to GPU classification; the current dropped-reference stat is expected to stay at zero under today's shared caps because the total world-light limit matches the per-tile light-index budget
* for Metal shader-converter pipelines, offline reflection JSON should be treated as a layout hint rather than a final authority for encoder bind indices; actual bind points still need confirmation against working backend patterns, validation output, and runtime behavior
* the current milestone 25 slice now uses renderer-owned world render-item extraction, GPU visibility compaction, and indirect textured world draws on Vulkan, Metal, and DirectX 12, while world text, UI, overlay debug, composite, and log console rendering intentionally remain on simpler direct paths
* shader compilation now needs to stay honest about shared include/header dependencies; cross-machine rebuilds should not rely on top-level `.hlsl` timestamps alone when shared shader-side contracts change
* the current milestone 26 slice now adds renderer-owned composite fullscreen-pass orchestration plus a captured battle-swirl transition path on Vulkan, Metal, and DirectX 12; the validated swirl path includes auxiliary-window behavior where that presentation surface is supported

---

## 6. Engineering Expectation

New renderer or RHI work should preserve this rule:

**If a feature is treated as part of the current supported renderer slice, parity expectations should be written down and validated as part of the work.**

That means:

* grow the renderer through the context-level contract unless a lower-level exception is clearly justified
* keep shared limits explicit
* document when support is current, partial, or pending platform-specific validation
