# Carrot Practical RHI Contract

**BunnySoft**
**Current engine slice audit**
**Last Updated: April 18, 2026**

---

## 1. Purpose

This document records the practical RHI contract Carrot's renderer actually uses today.
It also records the minimum compute-capable expansion milestone 23 is allowed to treat as the next real shared contract.

It exists so backend parity work can be guided by the live engine path instead of a larger, partly legacy abstraction surface.

This is not a promise that the RHI will never expand.
It is a statement of what the current renderer slice truly depends on and what the next renderer growth step is allowed to depend on once milestone 23 lands.

See also:

* [backend_support_current_state.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/backend_support_current_state.md:1) for the broader current-state support note
* [backend_parity_current_slice_checklist.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/backend_parity_current_slice_checklist.md:1) for the engineering validation checklist tied to this contract

---

## 2. Practical Live Contract

The current renderer slice is centered on `rhi_context_t`.

The practically exercised surface is:

* frame lifecycle
  * `begin_frame()`
  * `record_textured_quad_stage(...)`
  * `record_text_quad_stage(...)`
  * `end_frame()`
* resource creation through the context
  * `create_texture_2d(...)`
  * `create_buffer(...)`
  * `create_compute_pipeline(...)`
  * `create_sampler(...)`
  * `get_or_create_sampler(...)`
  * `bind_textured_quad_resources(...)`
* compute execution through the context
  * `dispatch_compute(...)`
* presentation and resize support
  * `resize(...)`
  * `add_presentation_window(...)`
  * `remove_presentation_window(...)`
  * `wait_idle()`
* backend identity / host access
  * `get_graphics_api()`
  * `get_swapchain()`
  * `get_command_queue()`
  * `get_device()` only as a backend-owned/legacy access seam, not as the practical renderer contract

For the current supported renderer slice, parity should be evaluated primarily against that context-level surface.

---

## 3. Currently Supported Renderer Slice

The parity-relevant renderer slice currently includes:

* world textured quad rendering
* text quad rendering
* UI/debug/log/composite stage submission
* render-target and viewport-space stage handling
* auxiliary presentation windows where currently implemented
* texture, buffer, and sampler creation needed by those paths

This is the slice that backend parity claims should be tied to today.

---

## 4. Milestone 23 Audit: Current Contract Gaps

The current practical contract is no longer purely graphics-centric, but it is still intentionally narrow.

It still does not yet provide shared renderer-facing support for:

* storage or structured GPU-written buffers
* indirect argument buffers
* explicit renderer-visible resource-state transitions
* explicit compute-to-graphics or graphics-to-compute handoff rules

That gap is visible in the current shared headers:

* `rhi_context_t` now exposes compute pipeline creation and dispatch, but the milestone slice still limits that compute work to a narrow before-graphics frame path
* `buffer_usage_t` now includes `storage`, `indirect`, and `readback`, but later passes still need explicit resource-state/synchronization rules
* `rhi_command_queue_t` names `graphics`, `compute`, and `transfer` queue types, but the practical renderer contract still runs the first compute slice through the live context path rather than a broader queue-management abstraction

Milestone 23 should therefore be treated as a contract-growth milestone, not as permission to route compute work through backend-local escape hatches.

---

## 5. Compute-Capable Contract Target For Milestone 23

The minimum honest next contract should stay context-centric.

The renderer-facing shared additions should be narrow:

* context-level compute pipeline creation and ownership
* context-level dispatch entry points
* shared buffer usage expression for storage-style GPU-written data
* shared buffer usage expression for indirect argument data
* readback support only where diagnostics or validation genuinely require it
* explicit pass-boundary/resource-state expectations for data written in one pass and consumed in a later pass
* one narrow indirect draw path suitable for later world execution growth

The currently landed indirect path for that rule is:

* `indirect_textured_quad_stage_record_t`
* `rhi_context_t::record_indirect_textured_quad_stage(...)`
* one indexed indirect draw command per recorded stage
* one explicit texture/sampler binding per recorded stage

The currently landed pass-boundary rule is:

* compute dispatches in the live milestone 23 slice are declared as `before_graphics`
* the caller explicitly declares whether later graphics work will read compute-written storage data
* backends must honor that declared compute-to-graphics handoff without requiring renderer-local backend exceptions

The core rule is:

* if the renderer needs compute, storage, indirect, or pass-boundary synchronization behavior, that behavior should be expressed through `rhi_context_t` and shared resource descriptions before native backends use backend-local side doors

### 5.1 Minimum Shared Additions

Milestone 23 does not need a giant general-purpose command API.
It does need a few new shared concepts.

The minimum addition set should cover:

* resource description growth for storage/structured buffers
* resource description growth for indirect argument buffers
* readback-capable buffers where diagnostics or validation justify them
* a shared compute pipeline concept
* dispatch support at the context level
* explicit write-then-read handoff expectations across pass boundaries
* explicit compute-to-graphics and graphics-to-compute expectations where later milestones need them
* one shared indirect draw form rather than a broad family of indirect commands

The currently live shared declaration surface for that is:

* `compute_dispatch_order_t`
* `compute_graphics_handoff_t`
* `compute_dispatch_record_t::graphics_handoff`

### 5.2 What Should Stay Out Of Scope

Milestone 23 should still avoid overreaching into abstractions the engine does not need yet.

It should not require:

* a giant command-list rewrite
* a fully generic descriptor system
* a backend-agnostic frame graph
* a broad indirect-command generation surface
* a bindless-first renderer contract

The contract should grow only enough to support the next real renderer milestones honestly.

---

## 6. Parity Expectations For The Compute-Capable Slice

When milestone 23 lands, parity for Vulkan, Metal, and DirectX 12 should mean all three native backends can support the same narrow compute-capable contract, not merely compile shared types.

That parity expectation should include:

* all three backends accept the same shared buffer/resource usage descriptions for storage, indirect, and justified readback paths
* all three backends expose the same context-level compute dispatch behavior to the renderer
* all three backends honor the same pass-boundary ownership and resource-transition expectations for the milestone slice
* all three backends support the same narrow indirect draw form the renderer is allowed to target
* null-backend or other validation scaffolding protects the shared contract shape even when native visual validation remains partly manual
* all three backends honor the same explicit `before_graphics` compute-to-graphics handoff declaration for the current slice
* all three backends treat the live indirect path as a textured-quad world-stage path rather than a broad generic command family

This parity claim should remain intentionally narrow:

* it is about the compute-capable renderer slice Carrot actually uses
* it is not a blanket claim that every backend exposes identical low-level helpers or the same internal synchronization strategy

---

## 7. Surface Honesty Findings

### 7.1 Context-Centric Reality

The live engine path is context-centric, not device-centric, and milestone 23 should preserve that direction.

That is visible in practice because:

* `renderer_t` talks to `rhi_context_t`
* textured/text stage recording is backend-owned through the context
* runtime resource growth for quad stages goes through context-level `create_buffer(...)`
* texture and sampler creation also go through the context

### 7.2 Former `rhi_device_t` Breadth

Before milestone 22 cleanup, `rhi_device_t` exposed a broader factory-style surface:

* `create_command_queue(...)`
* `create_swapchain(...)`
* `create_buffer(...)`
* `create_texture()`
* `create_graphics_pipeline()`
* `destroy_buffer(...)`

That broader surface did not map cleanly to the practical renderer path, and several backends only carried it as stale or stub-shaped implementation weight.

The shared `rhi_device_t` contract is now reduced to the low-level device object itself.
If a backend still wants helper methods such as queue or swapchain construction, those helpers now live as backend-local methods on the concrete device type rather than as inherited parity promises.

### 7.3 Uneven Backend Reality

Metal and DirectX 12 previously exposed legacy `rhi_device_t` methods that returned `nullptr` or otherwise acted as stubs, even though both backends participated in real rendering through their `rhi_context_t` implementations.

Concrete examples:

* [src/Engine/RHI/Backends/Metal/MetalDevice.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/RHI/Backends/Metal/MetalDevice.cpp:39)
* [src/Engine/RHI/Backends/DirectX12/DirectX12Device.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/RHI/Backends/DirectX12/DirectX12Device.cpp:89)

That mismatch was a strong sign that the shared device abstraction was overstating practical parity more than the context abstraction was.

---

## 8. Audit Conclusion

The honest current contract is:

* `rhi_context_t` is the practical backend contract for the live renderer slice
* `rhi_device_t` is a backend-owned low-level object, not a shared renderer-facing factory contract
* any remaining queue/swapchain helper methods belong to concrete backend device classes, not the shared parity surface
* parity claims should be attached to the context-level renderer slice that is actually exercised

The honest milestone 23 direction is:

* compute growth should extend the context-level contract rather than bypass it
* buffer/resource descriptions should grow before compute-heavy renderer features do
* resource-state and indirect execution support should be introduced through one narrow shared path
* Vulkan, Metal, and DirectX 12 parity expectations should be attached to that compute-capable context slice as it lands

This should guide milestone 23 work:

* keep misleading device-level parity expectations out of the shared contract
* preserve parity across Vulkan, Metal, and DirectX 12 for the context-level renderer slice
* avoid growing new renderer features on top of backend surfaces that are not honestly real yet

## Shared Constraint Ownership

The current renderer slice also depends on a few shared limits that should be treated as
contract-level inputs rather than backend-local implementation details:

* forward+ lighting limits come from generated `ForwardPlusSharedConfig.h`, which is compiled into renderer code, native backends, and shader builds together
* `rhi::k_max_textured_quad_stage_slots_per_frame` is a shared stage-slot budget that every backend provisions against for textured/text stage resources
* presentation routing currently assumes the known channel mask defined by `presentation_channel_gameplay` and `presentation_channel_log_console`

These are not "just constants." They are part of the practical parity slice because changing
them changes what the renderer can legally ask every backend to do.
