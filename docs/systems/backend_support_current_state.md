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

The current parity story is therefore:

* honest for the current renderer slice
* intentionally narrower than a blanket claim of complete backend sameness

---

## 4. Current Validation Shape

Carrot currently validates backend parity through a mix of automated and manual checks.

Automated support currently protects the shared renderer contract through:

* null-backend stage recording regressions
* presentation-channel and auxiliary-window delegation tests
* shared limit validation around stage slots, presentation routing, and forward+ limits

Manual validation still matters for native output:

* Vulkan should be exercised against the current Sandbox slice
* Metal should be exercised against the current Sandbox slice
* DirectX 12 should be exercised on Windows against the same slice

At the time of this note:

* Vulkan and Metal have been manually exercised against the current renderer slice after milestone 22 contract cleanup
* DirectX 12 remains part of the supported slice, but native validation should still be rerun on Windows whenever milestone-closeout confidence is needed

---

## 5. Important Current Limitations

Current limitations and cautions include:

* parity claims are attached to the current context-level renderer slice, not every historical RHI surface
* backend-local helper methods are allowed where useful, but they are not shared parity promises by default
* future renderer work still needs discipline so new features do not quietly become one-backend-first
* native backend pixel-output validation is still partly manual outside the null-backend regression harness

---

## 6. Engineering Expectation

New renderer or RHI work should preserve this rule:

**If a feature is treated as part of the current supported renderer slice, parity expectations should be written down and validated as part of the work.**

That means:

* grow the renderer through the context-level contract unless a lower-level exception is clearly justified
* keep shared limits explicit
* document when support is current, partial, or pending platform-specific validation
