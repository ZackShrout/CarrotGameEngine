# Carrot Practical RHI Contract

**BunnySoft**
**Current engine slice audit**
**Last Updated: April 18, 2026**

---

## 1. Purpose

This document records the practical RHI contract Carrot's renderer actually uses today.

It exists so backend parity work can be guided by the live engine path instead of a larger, partly legacy abstraction surface.

This is not a promise that the RHI will never expand.
It is a statement of what the current renderer slice truly depends on.

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
  * `create_sampler(...)`
  * `get_or_create_sampler(...)`
  * `bind_textured_quad_resources(...)`
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

## 4. Surface Honesty Findings

### 4.1 Context-Centric Reality

The live engine path is context-centric, not device-centric.

That is visible in practice because:

* `renderer_t` talks to `rhi_context_t`
* textured/text stage recording is backend-owned through the context
* runtime resource growth for quad stages goes through context-level `create_buffer(...)`
* texture and sampler creation also go through the context

### 4.2 Former `rhi_device_t` Breadth

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

### 4.3 Uneven Backend Reality

Metal and DirectX 12 previously exposed legacy `rhi_device_t` methods that returned `nullptr` or otherwise acted as stubs, even though both backends participated in real rendering through their `rhi_context_t` implementations.

Concrete examples:

* [src/Engine/RHI/Backends/Metal/MetalDevice.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/RHI/Backends/Metal/MetalDevice.cpp:39)
* [src/Engine/RHI/Backends/DirectX12/DirectX12Device.cpp](/Users/zshrout/dev/CarrotGameEngine/src/Engine/RHI/Backends/DirectX12/DirectX12Device.cpp:89)

That mismatch was a strong sign that the shared device abstraction was overstating practical parity more than the context abstraction was.

---

## 5. Audit Conclusion

The honest current contract is:

* `rhi_context_t` is the practical backend contract for the live renderer slice
* `rhi_device_t` is a backend-owned low-level object, not a shared renderer-facing factory contract
* any remaining queue/swapchain helper methods belong to concrete backend device classes, not the shared parity surface
* parity claims should be attached to the context-level renderer slice that is actually exercised

This should guide milestone 22 work:

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
