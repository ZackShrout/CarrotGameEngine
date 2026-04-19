# Backend Parity Current Slice Checklist

**BunnySoft**
**Current validation checklist**
**Last Updated: April 19, 2026**

---

## 1. Purpose

This checklist captures what backend parity means for Carrot's current renderer slice.

It is intentionally narrower than "everything the RHI could someday support."
The goal is to keep parity tied to the renderer behavior Carrot currently treats as real.

See also:

* [rhi_contract_current_slice.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/rhi_contract_current_slice.md:1) for the contract this checklist is protecting
* [backend_support_current_state.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/backend_support_current_state.md:1) for the current-state support wording used in top-level docs

---

## 2. Current Parity Slice

The current parity slice includes:

* world textured-quad rendering
* world text-quad rendering
* UI stage rendering
* overlay debug stage rendering
* composite stage rendering
* log console stage rendering
* context-level texture, buffer, and sampler creation
* resize handling
* auxiliary presentation window registration where a backend currently supports it

This slice does not claim that every backend exposes identical low-level helper methods.
It claims that the current renderer-facing behavior remains aligned across Vulkan, Metal, and DirectX 12.

---

## 3. Engineering Expectations

New renderer work should preserve these rules:

* world rendering remains the only lighting-aware stage
* UI, overlay debug, composite, and log console remain unlit non-world stages
* log console rendering stays on `presentation_channel_log_console`
* gameplay-facing stages stay on `presentation_channel_gameplay`
* text rendering remains part of the same practical cross-backend slice as textured-quad rendering
* auxiliary presentation support should remain explicit rather than silently assumed
* shared compute pipeline creation and dispatch should remain parity-relevant through the same context-level contract as the graphics slice
* compute-to-graphics handoff should be declared explicitly rather than inferred from backend-local command ordering
* the current indirect path should stay limited to the shared textured-quad indexed-indirect contract unless a wider contract is deliberately documented

## 3.1 Shared Limits That Belong To The Contract

The following limits are shared renderer-contract inputs, not backend-local tuning knobs:

* forward+ world-light and tile caps from the checked-in shared `Renderer/Draw/ForwardPlusSharedConfig.h`
* `rhi::k_max_textured_quad_stage_slots_per_frame`
* the current known presentation-channel mask (`gameplay` and `log_console`)

If one of these changes, parity work is not done until:

* renderer validation is still honest
* Vulkan, Metal, and DirectX 12 resource provisioning still matches
* the affected docs and milestone notes are updated

Current visibility expectations:

* world-light overflow should surface through renderer stats rather than disappearing silently
* forward+ dropped light references should remain visible in renderer stats/debug output, even though the current shared caps mean that value is normally expected to stay at zero
* stage routing should continue to validate against known presentation channels

---

## 4. Current Validation Sources

### 4.1 Automated Regression Support

Current automated support includes:

* null-backend renderer stage recording via [src/Engine/RHI/Backends/Null/NullRHIContext.h](/Users/zshrout/dev/CarrotGameEngine/src/Engine/RHI/Backends/Null/NullRHIContext.h:16)
* null-backend compute and indirect contract regressions in [tests/RHIComputeTests.cpp](/Users/zshrout/dev/CarrotGameEngine/tests/RHIComputeTests.cpp:12)
* shared buffer-usage contract regressions in [tests/RHIBufferTests.cpp](/Users/zshrout/dev/CarrotGameEngine/tests/RHIBufferTests.cpp:1)
* renderer stage-space and presentation-mask regressions in [tests/SceneLoadingTests.cpp](/Users/zshrout/dev/CarrotGameEngine/tests/SceneLoadingTests.cpp:2750)
* runtime window/presentation-channel expectations in [tests/WindowSystemTests.cpp](/Users/zshrout/dev/CarrotGameEngine/tests/WindowSystemTests.cpp:12)

These tests do not prove native backend pixel output directly.
They do protect the shared renderer contract that native backends are expected to implement.

### 4.2 Native Backend Manual Checks

At milestone closeout, the practical native checks should cover:

* Vulkan still renders the current Sandbox slice correctly
* Metal still renders the current Sandbox slice correctly
* DirectX 12 still renders the current Sandbox slice correctly when validated on Windows
* current multi-window behavior still behaves correctly where supported

Current milestone 24 validation snapshot:

* Vulkan manual validation is current as of **April 19, 2026**
* Metal manual validation is current as of **April 19, 2026**
* DirectX 12 shared-code compilation is current as of **April 19, 2026**
* DirectX 12 native runtime validation is still pending a follow-up Windows rerun, so milestone 24 closeout should remain visible in `docs/` rather than archived yet

---

## 5. Failure Conditions

Parity for the current slice should be considered at risk if:

* a renderer stage changes presentation mask or space without corresponding backend validation
* one backend starts requiring a special renderer path for world/text/UI/log behavior
* a backend keeps compiling but no longer supports a stage the renderer treats as part of the current slice
* backend-local helper growth starts reintroducing a fake shared contract that other backends do not actually implement
* compute/storage/indirect work begins shipping through backend-local escape hatches before the shared milestone 23 contract lands

---

## 6. Current Interpretation

Backend parity at this stage means:

* Vulkan, Metal, and DirectX 12 all participate in the same practical renderer contract through `rhi_context_t`
* the engine keeps the supported renderer slice honest and shared
* backend-local implementation details are allowed, but renderer-facing behavior should remain aligned
