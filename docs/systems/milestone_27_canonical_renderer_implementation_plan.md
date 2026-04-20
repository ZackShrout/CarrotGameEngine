# Milestone 27 Canonical Renderer Implementation Plan

**BunnySoft**
**Implementation plan**
**Last Updated: April 20, 2026**

---

## 1. Purpose

This note locks the implementation direction for milestone 27.

It exists to answer the "how are we actually going to do this?" question in concrete terms before the renderer refactor begins in earnest.

This is not a compatibility plan.
It is an execution plan for replacing Carrot's mixed quad-rendering state with one canonical renderer-owned path.

See also:

* [milestone_27_canonical_quad_renderer_and_runtime_performance_2026.md](/Users/zshrout/dev/CarrotGameEngine/docs/milestone_27_canonical_quad_renderer_and_runtime_performance_2026.md:1)
* [backend_support_current_state.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/backend_support_current_state.md:1)
* [rhi_contract_current_slice.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/rhi_contract_current_slice.md:1)

---

## 2. Current Engine Reality

Carrot's current renderer is already split across two materially different execution paths:

* world textured quads flow through renderer-owned world-item extraction, GPU cull/compaction, and indirect textured draws
* world text, UI, composite, overlay debug, and log console quads still flow through stage-local CPU-expanded vertex/index generation and direct textured-quad batching

That means Carrot does **not** currently have one canonical quad renderer.
It has a stronger world path and several narrower older paths still coexisting beside it.

Milestone 27 exists to end that split.

---

## 3. Locked Decisions

The following implementation decisions are now adopted for milestone 27:

* Carrot is building one backend-neutral canonical quad renderer
* Vulkan is the first backend that will fully implement the new canonical path
* Metal and DirectX 12 may be temporarily stubbed while the new shared contract is in flight
* Metal and DirectX 12 must continue to compile during that transition window
* backend parity is restored before milestone closeout, but not preserved continuously during the deepest refactor phase
* compatibility layers should not be preserved merely to reduce short-term churn
* if current public or renderer-facing APIs make the canonical path less canonical, Milestone 27 may replace them directly

The core rule is:

**We are not preserving the old renderer architecture as a co-equal fallback while the new one lands.**

---

## 4. Validated Milestone Slice

Milestone 27's canonical renderer target includes the live 2D quad-facing slice:

* world textured quads
* world text quads
* UI textured quads
* UI text quads
* overlay debug textured quads
* overlay debug text quads
* log console textured quads
* log console text quads
* composite textured and solid quad/fullscreen work that belongs to the current 2D/composite substrate

This plan intentionally treats those as one renderer family with stage-local semantics, not as separate renderer architectures.

The slice does **not** require milestone 27 to solve:

* 3D rendering
* lights/shadows beyond preserving the current world-lighting slice
* shaft-light implementation
* a broad material-system rewrite
* every future post-effect type

---

## 5. Canonical Renderer Contract

### 5.1 Gameplay-Facing Rule

Gameplay-facing systems should continue to submit render intent:

* what to draw
* where to draw it
* which stage/space/target it belongs to
* what ordering semantics it requires

Gameplay-facing APIs are allowed to stay intent-shaped.
They do not need to expose bucket mechanics or backend execution details.

### 5.2 Renderer-Facing Rule

Inside the renderer, quad work should converge into one renderer-owned canonical instance stream.

That canonical stream should own:

* quad geometry meaning
* UV rectangle
* tint/effect parameters
* ordering/layering inputs
* stage identity
* target/pass routing identity
* texture/material/sampler identity
* lighting mode where relevant
* stable submission order for tie-breaking

### 5.3 Canonical Instance Fields

The exact struct name can change during implementation, but the canonical quad instance concept should carry at least:

* stage kind
* stage space / target identity
* texture handle or resolved texture identity
* sampler preset
* world-material key where world lighting needs it
* position and size in the authored stage coordinate space
* UV rect
* color
* effect mode and effect parameter payload needed by the current shaders
* render layer
* order mode
* order-in-layer
* sort reference Y
* lighting participation flag or equivalent world/non-world execution mode
* stable submission index

The renderer may store extra internal fields for diagnostics, packing, or bucket ownership as needed.

### 5.4 Canonical Bucket Identity

Two quad instances belong to the same execution bucket only when they agree on the fields that actually affect shared execution.

The default bucket key should be defined in backend-neutral terms and should include:

* stage kind
* target/pass identity
* lighting mode / world draw mode
* texture identity
* sampler preset
* world-material key when applicable
* text-vs-non-text distinction only if shader/pipeline reality still requires that split after the refactor

The bucket key should **not** encode backend-local concepts.

### 5.5 Canonical Ordering Rule

Ordering should remain renderer-owned and should continue to honor Carrot's current semantics:

* render layer ordering
* render order mode
* bottom-anchor Y sorting when requested
* explicit `order_in_layer`
* stable submission order as the final tie-breaker

That ordering should be applied before or while building canonical buckets so no backend has to reinvent ordering rules.

---

## 6. What Becomes Non-Canonical

The following current patterns should be treated as transitional and eligible for retirement during milestone 27:

* stage-local CPU expansion into per-frame quad vertices and indices as the default execution model
* keeping world indirect execution and non-world direct execution as equal-status long-term paths
* RHI recording surfaces whose shape primarily reflects the old direct-vs-indirect split instead of the new canonical instanced path
* carrying renderer-internal compatibility layers solely because older call sites were written around the previous execution model

Gameplay-facing draw calls may survive.
Renderer-internal execution structures do not receive the same protection.

---

## 7. Backend Strategy

### 7.1 Vulkan-First Rule

Vulkan is the first full implementation backend for the canonical renderer.

That means Vulkan will be the first backend expected to:

* implement the new canonical quad recording path
* validate bucketed instanced execution correctness
* validate performance instrumentation
* validate render-target/offscreen routing on the new contract

### 7.2 Metal and DirectX 12 Transition Rule

Metal and DirectX 12 remain first-class citizens in the architecture, but they may be temporarily suspended during the deepest refactor window.

During that window:

* both backends must continue to compile
* both backends may stub the new path
* stubs should fail explicitly and descriptively if the new path is reached
* the milestone should not pretend those backends are still validated during the suspension window

Acceptable temporary behavior includes:

* explicit `not yet re-enabled for milestone 27 canonical path` runtime failure
* compile-time placeholders that preserve interface conformance while withholding execution support

Unacceptable temporary behavior includes:

* silent fallback to stale legacy execution that makes the canonical path optional
* backend-specific renderer forks that bypass the shared contract
* leaving the engine in a state where it is unclear whether a backend is supported, suspended, or regressed

### 7.3 Backend Re-Enablement Order

After Vulkan stabilizes, milestone 27 should restore the other native backends one at a time.

The second backend should be chosen by learning value:

* choose Metal second if Vulkan assumptions need a stronger non-Vulkan abstraction test
* choose DirectX 12 second if the shared contract already looks portable and the faster path back to parity matters more

The final backend is restored last.

### 7.4 Closeout Rule

Milestone 27 is not complete until:

* Vulkan, Metal, and DirectX 12 are all back on the canonical shared path
* docs/checklists describe that restored truth honestly
* temporary suspension notes are removed or archived

---

## 8. Expected Shared Contract Changes

Milestone 27 should expect shared renderer/RHI contract changes.

This milestone is specifically allowed to break or replace:

* renderer-internal batch/state structs that assume CPU-expanded quad geometry
* the current split between direct textured-quad stage recording and indirect textured-quad stage recording
* public or semi-public renderer APIs whose semantics force the old architecture to remain the default

This milestone should avoid preserving old shapes simply because they already exist.

If a current API makes the canonical path less coherent, the preferred action is:

* redesign it
* migrate call sites
* delete the obsolete path

---

## 9. Planned RHI Direction

The exact final names can change during implementation, but the direction should be:

* the renderer records canonical bucketed instanced quad work through one shared context-level path
* the RHI should stop treating "direct textured quad stage" and "indirect textured quad stage" as the two equal long-term quad execution models
* backend-specific recording details remain inside backend implementations, not in renderer-facing API shape

The target is a shared recording surface whose input is closer to:

* canonical quad bucket metadata
* shared quad instance buffers
* shared static quad mesh basis or equivalent instance-driven geometry source
* stage/viewport/presentation routing
* render-target/pass routing

The target is **not**:

* a Vulkan-specific draw packet model exposed as a shared API
* a broad general-purpose frame graph
* a giant generic command-list abstraction

---

## 10. Render-Target and Composite Direction

Milestone 27 should preserve and deepen the renderer-owned composite/offscreen seams added in milestone 26.

The canonical quad renderer must remain compatible with:

* explicit target ownership
* explicit target sizing/lifetime rules
* composite fullscreen pass routing
* captured/composited presentation behavior needed by current transition work

If the current composite structures need to change to fit the canonical path, that is acceptable.
What is not acceptable is losing the seam entirely or reverting to ad hoc presentation-only logic.

---

## 11. Performance and Diagnostics Plan

Milestone 27 should measure the renderer in a way that distinguishes:

* extraction cost
* sort/bucket build cost
* instance packing/upload cost
* world compute/cull cost
* draw recording cost
* presentation pacing cost
* non-renderer runtime cost such as world update, collision, and UI/runtime logic

The milestone should provide:

* capped vs uncapped/de-paced profiling distinction
* frame time in milliseconds
* benchmark discipline across light, representative, and stress scenes

The canonical renderer is not considered trustworthy if it is faster but still opaque.

---

## 12. Phased Execution Plan

### Phase 1. Canonical Contract Definition

Deliverables:

* define canonical quad instance concept
* define bucket identity
* define stage/target/pass routing rules
* define which current public APIs survive as intent APIs and which internal APIs are retired

Exit condition:

* the renderer team can explain exactly what the canonical path is in one page without referring to backend-local details

### Phase 2. Shared Renderer Refactor

Deliverables:

* route world, text, UI, composite, overlay, and log-console submissions into the canonical renderer-owned intermediate representation
* demote or remove stage-local CPU-expanded execution as the default model
* keep composite/offscreen routing explicit while the submission model changes

Exit condition:

* the renderer has one canonical intermediate representation even if only one backend executes it fully

### Phase 3. Vulkan Canonical Execution

Deliverables:

* Vulkan implements the bucketed instanced quad path
* correctness is validated on the live milestone slice
* Vulkan becomes the first performance-truth backend for the new architecture

Exit condition:

* the canonical path is real, not theoretical, on Vulkan

### Phase 4. Instrumentation and Performance Audit

Deliverables:

* frame-time and stage-timing instrumentation
* extraction/batching/submission diagnostics
* benchmark scene discipline

Exit condition:

* Carrot can say where frame time is going instead of guessing

### Phase 5. Metal or DirectX 12 Re-Enablement

Deliverables:

* restore one temporarily suspended backend on the shared canonical contract
* refine shared abstractions where backend two exposes hidden assumptions

Exit condition:

* two native backends are live on the same canonical path

### Phase 6. Final Backend Restoration and Cleanup

Deliverables:

* restore the last backend
* remove temporary suspension/stub scaffolding
* refresh parity docs/checklists
* retire remaining obsolete renderer paths

Exit condition:

* all three native backends are back on one documented canonical renderer path

---

## 13. Decision Rules During Implementation

When implementation tradeoffs appear, milestone 27 should use these rules:

* prefer canonical shared ownership over short-term compatibility
* prefer renderer-owned ordering/bucketing rules over backend-local special cases
* prefer explicit temporary backend suspension over silent stale fallback
* prefer replacing misleading APIs over wrapping them forever
* prefer documenting temporary unsupported status over implying parity that does not exist yet

---

## 14. Completion Standard

Milestone 27 is complete only when:

* Carrot has one documented canonical quad renderer path
* the validated 2D slice executes through that path by default
* Vulkan, Metal, and DirectX 12 all participate in that path again
* performance truth is available in frame-time terms
* composite/offscreen readiness remains intact for future shaft-light/post growth

Until then, Milestone 27 should be treated as a deliberate open-heart renderer refactor rather than a sequence of compatibility-preserving mini-patches.
