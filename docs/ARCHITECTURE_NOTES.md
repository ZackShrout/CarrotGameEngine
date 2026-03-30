# Carrot Game Engine – Architecture Notes

**BunnySoft**
**Working architecture notes**
**Last Updated: March 2026**

---

## 1. Purpose of This Document

This document captures the current architectural direction of Carrot Game Engine at a more technical level than `CARROT_MASTER_PLAN.md`, while still remaining high-level enough to evolve with the project.

It exists to preserve important design intent, subsystem boundaries, and implementation philosophy without pretending every detail is already finalized.

This is **not** a rigid spec and **not** a replacement for source code.
It is a guide to how Carrot is intended to be structured as the engine grows.

For project-wide direction, see `CARROT_MASTER_PLAN.md`.
For style and code rules, see `CODING_STANDARDS.md`.

---

## 2. Architectural Principles

A few core principles shape the engine’s architecture.

### 2.1 Native-First Systems

Carrot prefers native platform APIs and direct ownership of engine systems over broad third-party runtime abstraction layers.

This applies to:

* windowing
* graphics backends
* audio backends
* file/path behavior
* low-level engine integration

The goal is not to reject third-party code categorically, but to avoid making the engine dependent on large runtime frameworks where direct platform integration is both achievable and desirable.

### 2.2 Clear Layer Boundaries

Carrot is intended to be built in layers.

Important examples:

* platform code should not leak across the engine unchecked
* higher-level renderer logic should not collapse directly into backend API code
* asset definitions should not be confused with runtime loaded assets
* authored content should remain distinct from future cooked data
* world/gameplay systems should not be forced into the engine before the engine is ready for them

### 2.3 Explicit Over Magical

Subsystems should favor:

* explicit ownership
* readable control flow
* clear resource lifetimes
* understandable data flow
* practical abstractions

Carrot should avoid architecture that is “clever” but difficult to reason about.

### 2.4 Evolve When the Engine Is Ready

Not every future system needs to exist immediately.

A recurring design rule in Carrot is:

> build the next layer when the current layer actually needs it

This is especially important for:

* ECS
* scene/world systems
* editor/tooling
* cooked asset pipelines
* save/persistence architecture

---

## 3. Layered Engine Structure

Carrot is intended to evolve through a layered architecture rather than a monolithic one.

At a high level, the engine currently trends toward the following structure:

1. **Platform Layer**
2. **Core Systems**
3. **RHI Layer**
4. **Renderer Layer**
5. **Asset System**
6. **Audio System**
7. **Future World / ECS Layer**
8. **Game Layer**

These layers are not meant to be dogmatic, but they provide a useful structural model.

---

## 4. Platform Layer

The platform layer is responsible for native OS-facing behavior.

### Responsibilities

* native window creation and lifetime
* event pump integration
* platform-specific input translation
* platform-specific path and executable handling
* platform-specific hooks required by rendering or audio systems
* fullscreen, focus, resize, and related window behavior

### Current / Planned Backends

* **Win32**
* **Wayland**
* **Cocoa**
* **X11** (planned)

### Notes

* Wayland is currently the primary Linux target
* X11 is intended to be brought up as an additional/fallback Linux backend later
* platform code should stay localized rather than spreading platform conditionals throughout higher-level engine code

The platform layer should expose stable engine-facing interfaces wherever practical, while still allowing native behavior to be handled correctly per OS.

---

## 5. Core Systems

Core systems provide the engine foundation that other layers rely on.

### Typical Core Responsibilities

* application / engine lifetime
* module startup and shutdown
* logging
* engine config loading
* event types and dispatch
* delegates / callback helpers
* debug helpers
* memory utilities
* file utilities
* hot-reload utilities
* virtual file system support

### Design Direction

Core systems should remain:

* lightweight
* dependable
* reusable
* low-surprise

They should avoid becoming a dumping ground for unrelated features.

A good rule is:

* if a system is broadly foundational, it probably belongs here
* if it is domain-specific to rendering, assets, audio, or gameplay, it probably belongs elsewhere

---

## 6. RHI Layer

The RHI is the engine’s rendering hardware abstraction boundary.

Its purpose is not to hide that graphics APIs are different. Its purpose is to give the engine a stable set of rendering concepts to build against.

### Responsibilities

The RHI should expose concepts such as:

* device
* swapchain
* command queue
* command list / command buffer
* buffer
* texture
* sampler
* pipeline
* fence
* semaphore or equivalent synchronization primitive

### Design Intent

The RHI exists so that:

* the renderer does not directly depend on Vulkan, Metal, or DirectX 12
* higher-level rendering systems can be written once against engine-owned concepts
* backend-specific details remain contained inside backend implementations

### Important Boundary

The RHI should stay focused on **rendering hardware concepts**, not absorb higher-level renderer policy.

That means the RHI should not become a catch-all for:

* scene logic
* batching policy
* sprite systems
* animation systems
* debug UI policy

Those belong above the RHI.

### Backend Direction

Current / planned native backends:

* **Vulkan**
* **Metal**
* **DirectX 12**

The long-term goal is practical backend parity where the higher-level engine can rely on a coherent set of rendering behaviors.

---

## 7. Renderer Direction

The renderer sits above the RHI and is responsible for actual engine-facing rendering behavior.

### Near-Term Direction

The current renderer direction is intentionally modest and foundational.

Key current priorities include:

* textured quad rendering
* resource binding behavior
* batch-oriented 2D rendering foundations
* backend parity
* frame lifecycle clarity
* debug-friendly rendering flow

This stage is intentionally about building a solid base, not about racing into every future rendering feature immediately.

### Renderer vs RHI

This distinction matters:

* **RHI** = rendering hardware abstraction
* **Renderer** = engine rendering behavior and policy

Examples of renderer-level concerns:

* batching
* draw ordering
* sprite rendering
* camera/projection handling
* debug rendering
* future text/UI rendering
* world-facing rendering flow

### Intended Evolution

The renderer is expected to evolve roughly along this path:

1. basic backend bring-up
2. textured quad rendering
3. batched 2D rendering
4. sprite rendering and animation-aware drawing
5. camera / projection support
6. debug overlays / text
7. world-driven rendering flow

That progression is intentional.

Carrot should not skip foundational rendering steps in pursuit of more advanced features before the basics are robust.

---

## 8. Shader Build Pipeline

Carrot’s shader system is built around a **single shared shader source policy**.

### 8.1 Core Shader Policy

Carrot should maintain **one engine-facing shader source set**, not separate independently-authored shader codebases for Vulkan, Metal, and DirectX 12.

The canonical shader language for Carrot is **HLSL**.

This is an intentional architectural policy.

The goal is to ensure that:

* shader logic is authored once
* rendering behavior remains aligned across backends
* long-term shader maintenance stays practical
* backend parity does not require maintaining multiple divergent shader implementations

### 8.2 Canonical Source vs Derived Outputs

In Carrot, HLSL source files are the **authored shader sources**.

Backend-specific shader outputs are treated as **derived artifacts**, not primary sources.

Examples include:

* **SPIR-V** for Vulkan
* **DXIL** for DirectX 12
* **`.metallib`** outputs for Metal

This distinction matters.

The engine should think in terms of:

* one authored shader source set
* one canonical shader language
* multiple backend outputs generated by the build pipeline

### 8.3 Current Build Pipeline

Carrot’s current shader compilation flow is centered around **DXC** as the main shader compiler.

The current backend-specific flows are:

#### Vulkan

* HLSL source
* compiled with **DXC**
* output as **SPIR-V**
* Vulkan consumes the generated `.spv` output

This currently uses DXC’s SPIR-V path, including Vulkan-oriented compilation flags.

#### DirectX 12

* HLSL source
* compiled with **DXC**
* output as **DXIL**
* DirectX 12 consumes the generated `.dxil` output

This keeps DirectX 12 on a native HLSL/DXIL path.

#### Metal

* HLSL source
* compiled with **DXC** to **DXIL**
* DXIL is passed through **`metal-shaderconverter`**
* final output is a **`.metallib`** file

Carrot’s current Metal shader path also produces reflection JSON from the Metal conversion step.

### 8.4 Why This Pipeline Exists

This shader policy exists to prevent:

* duplicated shader logic across graphics backends
* backend drift over time
* inconsistent rendering behavior caused by separate shader implementations
* unnecessary long-term maintenance overhead

It also gives Carrot a much cleaner cross-backend rendering story:

* one renderer-facing shader model
* one authored shader language
* one build pipeline concept
* many backend outputs

### 8.5 Backend-Specific Defines and Translation Boundaries

Even with a shared source policy, some backend-specific compile-time definitions or accommodations are expected.

That is acceptable.

What Carrot should avoid is not all backend-specific handling, but rather **fully separate hand-maintained shader codebases**.

The intended rule is:

> backend-specific compilation details are acceptable
> backend-specific authored shader divergence should be minimized

### 8.6 Architectural Intent

Shader compilation should remain part of Carrot’s broader architecture, not just a loose collection of build commands.

This means the shader pipeline should continue to align with:

* backend parity goals
* shader hot-reload workflows
* asset / shader path conventions
* engine-facing shader usage patterns
* long-term maintainability

As Carrot evolves, shader tooling may improve, but the core policy should remain:

> **one shader source set, one canonical authored language, multiple derived backend outputs**

---

## 9. Asset System

The asset system is one of Carrot’s most important architectural pillars.

### 9.1 Asset Categories

Carrot should clearly distinguish between four different things:

#### Source Files

Original authored media or source data, such as:

* `.png`
* `.wav`
* shader source files
* future sprite sheets, tile maps, fonts, etc.

#### Authored Asset Definitions

Human-authored metadata files that tell the engine what an asset is and how it should be interpreted.

Examples:

* `*.texture.json`
* `*.audio.json`
* future `*.sprite.json`, `*.tilemap.json`, etc.

These are not the runtime assets themselves.

#### Runtime Loaded Assets

In-memory engine objects built from source files and authored metadata.

Examples:

* decoded image data
* loaded audio sample data
* streaming audio objects
* GPU texture objects
* higher-level loaded asset handles

#### Future Cooked / Cached Artifacts

Optional machine-generated derived data that may become useful later.

Examples:

* preprocessed texture caches
* pre-resampled audio caches
* atlas data
* animation caches
* backend-specific derived artifacts

These are not required to exist now, but the asset architecture should leave room for them later.

### 9.2 Asset Philosophy

Carrot assets should remain:

* explicit
* inspectable
* tool-friendly
* portable across engine/game roots
* understandable in source control

### 9.3 Virtual File System

Carrot uses a virtual file system so asset references are not tied directly to fragile raw paths.

Planned/current roots include:

* `engine://`
* `game://`
* `source://`
* `save://`

This provides a stable engine-facing asset addressing model and keeps engine/game content separation clear.

### 9.4 Importer vs Loader Split

This distinction is important and should be preserved.

#### Importers

Importers are responsible for understanding authored asset definitions and registering asset metadata.

They answer questions like:

* what is this asset?
* what source file does it depend on?
* what asset ID does it register?
* what authored settings apply?

Importers should not necessarily perform all expensive runtime loading work immediately.

#### Loaders

Loaders are responsible for producing usable runtime asset objects.

They answer questions like:

* decode the PNG
* open and decode or stream the audio
* resample audio if required
* construct runtime asset data
* create GPU-facing or engine-facing loaded representations

This split helps keep authored metadata handling separate from actual runtime loading work.

### 9.5 Serialization Philosophy

Carrot currently uses JSON for authored asset definitions because it is:

* human-readable
* easy to diff
* easy to validate
* easy to integrate with external tools

This is the correct fit for authored metadata right now.

However, JSON should not be forced to carry every kind of serialized data forever.

A useful distinction:

* **authored text serialization** → JSON is a strong fit
* **runtime save/load serialization** → separate concern, may evolve independently
* **cooked/cache serialization** → may eventually justify custom engine-native binary formats

Carrot should only introduce engine-native cooked formats like `.ctex`, `.caud`, and similar formats when they solve a real problem such as:

* startup cost
* repeated import cost
* platform-specific derived data
* packaging simplicity
* runtime simplification

They are not needed merely to appear “engine-like.”

For more detail, see `docs/systems/asset_pipeline.md`.

---

## 10. External Content Tool Workflows

Carrot is intentionally designed to work well with established external content tools.

This is a major architectural stance, not an afterthought.

### 10.1 Aseprite

Aseprite is intended to be a first-class sprite authoring workflow for Carrot.

Planned use cases include:

* sprite sheet creation
* frame-based animation
* animation metadata export
* tag-driven animation workflows
* integration with sprite asset definitions

The goal is for Carrot to consume Aseprite-friendly exported data rather than requiring an engine-native sprite editor immediately.

### 10.2 Tiled

Tiled is intended to be a first-class tile map authoring workflow for Carrot.

Planned use cases include:

* tile layers
* object layers
* map metadata
* animated tiles
* tilesets
* larger world composition workflows

This includes support for serious Tiled usage, not just trivial single-map import.

Important long-term direction includes support for:

* multi-map world composition
* streaming-style overworld structures
* map boundaries and larger world layouts
* later integration into world/scene systems

### 10.3 Why This Matters

Using strong external tools well is better than building weak custom tools too early.

Carrot’s asset pipeline should integrate cleanly with external tools first, and only later replace or augment those workflows where there is a real benefit.

---

## 11. Audio System

Carrot’s audio engine is intended to be a serious subsystem, not a thin playback wrapper.

### 11.1 Audio Philosophy

Audio should support:

* real gameplay needs
* clean runtime control
* multiple playback models
* routing and processing
* future extensibility

It should be architecturally strong enough that it could eventually support richer tooling, not just “play one sound.”

### 11.2 Core Direction

The audio system is being built around:

* a fixed internal mix model
* a central mixer
* buses and routing
* voice-based playback
* sample playback and streaming playback
* platform backend abstraction
* DSP-friendly architecture

### 11.3 Architectural Importance

Audio is not merely a later milestone. It is one of Carrot’s identity systems.

That means it deserves:

* strong boundaries
* real architectural care
* integration with assets and future gameplay systems
* room to grow without becoming chaotic

### 11.4 Future Direction

Longer-term audio directions may include:

* stronger asset tooling
* richer runtime integration
* better authoring workflows
* potential future editor/middleware-style ideas if they become worthwhile

Those possibilities should remain open, but not drive premature complexity today.

For more detail, see `docs/systems/audio_engine.md`.

---

## 12. ECS and World Direction

Carrot is intended to eventually move toward a world-driven architecture.

This includes future systems such as:

* entity/component storage
* world state
* gameplay simulation
* renderable world objects
* audio emitters or world-facing audio integration
* scene/world organization

### 12.1 ECS Design Intent

When introduced, ECS should aim to be:

* data-oriented
* explicit
* performant
* understandable
* tightly integrated with real engine needs

The intended design direction favors:

* archetype-style organization
* sparse-set-friendly access patterns where useful
* minimal dispatch overhead
* clear ownership and structural changes

### 12.2 Timing

ECS should not be introduced merely because “engines are supposed to have one.”

Its natural place is when Carrot is ready to transition from:

* manually driven rendering/test scenes

to:

* actual world-driven rendering and gameplay-facing organization

That means ECS belongs alongside:

* simple scene/world rendering flow
* component-driven object organization
* gameplay-facing engine structure

### 12.3 Non-Goals

The ECS should not become:

* a justification for premature complexity
* a doctrine that infects every single subsystem
* a replacement for good engineering judgment

If a system benefits from ECS integration, it should integrate well.
If it does not, the architecture should remain practical.

---

## 13. Hot Reloading

Hot reloading is an important long-term quality-of-life direction for Carrot.

### Intended Uses

* shader reload workflows
* asset refresh workflows
* fast iteration loops
* future tooling/editor iteration improvements

### Architectural Notes

Hot reload should be treated as an architectural concern, not an afterthought hacked in later.

That means:

* subsystem boundaries should tolerate resource refresh
* assets should have clear ownership and replacement behavior
* rendering pipelines should support safe rebuild points
* engine modules should avoid global state patterns that make reload flows painful

Hot reloading does not need to be maximally ambitious immediately, but the engine should continue making choices that keep it feasible.

---

## 14. Config, Saves, and Serialization Boundaries

These concerns should remain distinct.

### 14.1 Config Data

Config data is engine/application configuration.

Examples:

* graphics API preference
* debug layer settings
* audio settings
* engine defaults
* future game/user override settings

Config should remain simple and explicit.

### 14.2 Asset Metadata

Asset metadata is authored content definition.

Examples:

* texture asset descriptors
* audio asset descriptors
* future sprite/tilemap descriptors

This is not the same as config, even when both use JSON.

### 14.3 Save / Persistence Data

Save data is future gameplay or runtime persistence.

Examples:

* player progress
* world state
* user save slots
* serialized game/session state

This is a separate problem space and should not be casually conflated with authored asset definitions.

### 14.4 Why This Separation Matters

A lot of systems become messy when:

* config
* authored assets
* derived cooked data
* runtime save state

all get blurred together.

Carrot should resist that blur.

---

## 15. Future Tooling / Editor Direction

Carrot is not currently dependent on a custom editor.

That is intentional.

### Current Direction

For now, Carrot should prioritize:

* strong engine systems
* strong runtime architecture
* good external tool integration
* good authored asset workflows

### Later Tooling Direction

Once the engine genuinely benefits from them, future tooling may include:

* asset browsers
* debug visualizers
* editor utilities
* content pipeline helpers
* engine-native workflow tools

But tooling should emerge from real engine needs rather than existing as speculative architecture.

A weak editor built too early is less valuable than a strong engine with clean external-tool workflows.

---

## 16. What This Document Should Preserve

As Carrot evolves, this document should continue preserving a few key ideas:

* native-first architecture matters
* layer boundaries matter
* RHI and renderer are not the same thing
* assets must distinguish source, authored metadata, runtime data, and future cooked data
* Aseprite and Tiled are intentional content workflow targets
* audio is a first-class subsystem
* ECS is important, but should arrive at the right time
* hot reload is a real architectural concern
* future tooling should be earned, not assumed

If those ideas remain intact, Carrot can evolve significantly without losing its identity.
