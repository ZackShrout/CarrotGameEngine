# Carrot Game Engine – Master Plan

**BunnySoft**
**Version 2.1**
**Last Updated: April 13, 2026**

---

## 1. Overview

Carrot Game Engine is a custom **native cross-platform C++23 game engine** being built primarily for the kinds of games I actually want to make.

It is designed first and foremost for:

* **desktop platforms**
* **2D and hybrid 2D/3D games**
* **RPG-style projects**
* **explicit control over engine systems**
* **clean, understandable architecture**

Carrot is not intended to be a general-purpose “do everything” engine competing with Unreal or Unity.

Its purpose is to be a **focused, long-lived, highly controllable engine** that serves its intended style of game development extremely well.

This document is the **high-level direction document** for Carrot.
It describes the project’s goals, architecture direction, and long-term development priorities.

For implementation details and evolving subsystem notes, see the additional project documentation.

---

## 2. Core Philosophy

Carrot is being built around a few foundational principles.

### Native First

Carrot talks directly to the underlying platforms and graphics/audio systems.

It intentionally avoids heavy runtime abstraction frameworks such as SDL or GLFW in favor of native windowing, input, rendering, and platform integration.

### Cross-Platform by Design

Windows, Linux, and macOS are all first-class targets.

Cross-platform support is not intended to be a later afterthought — it is part of the engine’s design from the beginning.

### Modern C++23

Carrot is written in modern C++ with a strong emphasis on:

* explicit ownership
* low-level control
* portability
* maintainability
* high-performance systems programming

### Focused Scope

Carrot is not trying to be everything to everyone.

It is being built specifically to support the workflows, systems, and game structures that matter most to its intended projects.

That means Carrot should prioritize being:

* coherent
* reliable
* pleasant to work in
* understandable
* powerful within its intended scope

…rather than bloated with features that exist only to mimic larger engines.

### Explicit Systems Over Magic

Rendering, assets, audio, input, and engine systems should be understandable and debuggable.

Carrot should prefer:

* explicit architecture
* clear ownership
* predictable runtime behavior
* strong subsystem boundaries

over opaque “engine magic.”

### Build Strong Foundations First

Carrot is intended to grow over time, but growth should happen on top of solid core systems.

Subsystems should be introduced when the engine is ready for them — not just because they sound impressive on paper.

---

## 3. What Carrot Is / Is Not

### Carrot Is

* a custom native engine
* a long-term personal / studio foundation
* focused on desktop game development
* intended to support strong 2D and hybrid workflows
* designed around explicit control and low overhead
* intended to remain understandable as it grows

### Carrot Is Not

* a clone of Unreal or Unity
* a general-purpose AAA production engine
* a mobile-first engine
* a console-focused engine
* a visual-scripting-first engine
* a framework that depends on giant runtime middleware to function

This distinction matters.

Carrot’s value comes from **clarity, control, and focus**, not from trying to check every possible engine feature box.

---

## 4. Platform Strategy

### Supported Target Platforms

Carrot is being built for:

* **Windows**
* **Linux**
* **macOS**

These three platforms are considered the primary long-term targets.

### Windowing Strategy

Carrot uses **native windowing backends** rather than a third-party abstraction layer.

Current platform windowing support:

* **Win32** on Windows
* **Wayland** on Linux
* **Cocoa** on macOS
* **X11** on Linux

Linux now supports both Wayland and X11 native windowing backends. Wayland remains the preferred/default Linux path when available, while X11 serves as the compatibility fallback backend.

### Graphics Backend Strategy

Carrot’s graphics architecture is built around a custom **Rendering Hardware Interface (RHI)**.

The long-term graphics backends are:

* **Vulkan**
* **Metal**
* **DirectX 12**

The goal is for higher-level engine rendering systems to remain independent from any one graphics API.

---

## 5. High-Level Engine Architecture

Carrot is being built in layers.

This layering is intended to keep platform-specific details, rendering APIs, asset handling, and gameplay-facing systems from collapsing into one another.

### 5.1 Platform Layer

Responsible for:

* native window creation
* OS-level integration
* platform-specific input translation
* platform file/path behavior
* platform-specific runtime hooks

This includes systems such as:

* Win32 windowing
* Wayland / X11 Linux windowing
* Cocoa windowing
* platform-specific file and executable path utilities

### 5.2 Core Systems

Core systems provide the foundational engine infrastructure.

These include:

* engine lifecycle / module ownership
* application host and game-runtime framework boundaries
* logging
* config loading
* event dispatch
* delegates / callbacks
* memory utilities
* file utilities
* debug helpers
* hot-reload support

These systems should remain lightweight, dependable, and broadly reusable across the engine.

Current runtime-framework direction inside core systems includes:

* `ce_application_t` as the low-level engine-facing application host boundary
* `game_runtime_t` as the engine-owned root for game-runtime-lifetime ownership
* `igame_state_t` as the explicit state seam between the runtime root and active gameplay/menu/pause/loading states

This structure is intended to keep application-shell responsibilities, game-runtime responsibilities, and gameplay-session responsibilities from collapsing into one object.

Recent engine/runtime growth also now includes:

* engine-owned multi-window runtime roles
* scene assets and scene runtime transition support
* default gameplay-facing controller helpers
* first-pass in-game UI runtime foundation

### 5.3 RHI Layer

The RHI is the low-level graphics abstraction boundary.

It is responsible for abstracting concepts such as:

* devices
* swapchains
* command queues / command lists
* buffers
* textures
* samplers
* pipelines
* synchronization primitives

The RHI exists so the renderer and higher-level engine systems do not directly depend on Vulkan, Metal, or DirectX 12.

### 5.4 Renderer Layer

The renderer sits above the RHI and is responsible for higher-level rendering behavior.

Its job is not just to issue API calls, but to provide a coherent rendering path for the engine.

Carrot already has a meaningful 2D-first renderer foundation in place, including:

* textured quad rendering
* batching
* explicit frame stages for world, UI, composite, debug, and runtime log-console responsibilities
* gameplay-facing presentation policies such as fixed-aspect letterboxing
* sprite rendering
* stable layered 2D draw ordering
* sprite placement controls such as pivot/origin and flip
* debug rendering and text
* engine-level full-screen composite overlays
* engine-owned in-game runtime UI built on top of the retained widget/navigation foundation
* a first-pass forward+ world-lighting path with engine-owned ambient and point-light support

The current renderer growth focus is narrower and more practical:

* preserve backend parity as renderer features expand
* keep the renderer / RHI boundary explicit and stable
* improve diagnostics, preview, and iteration surfaces on top of existing renderer stages
* support safe runtime reload of renderer-facing assets where practical
* continue moving more real game presentation through the engine-owned world/render path instead of sandbox bootstrap glue

Current renderer milestone status:

* the world renderer has crossed into a first-pass forward+ architecture
* world lighting is now an engine-owned runtime concern rather than sandbox-local proof code
* Vulkan, Metal, and DirectX 12 all participate in the current context-level renderer slice, including the current forward+ world path
* the current implementation is intentionally CPU-submission-driven, with CPU-built tiled light lists as the first-pass forward+ structure

Current parity wording should be read narrowly and honestly:

* parity is attached to the exercised renderer slice, not every conceivable low-level backend helper surface
* native backend validation still includes manual Sandbox checks where automated regressions cannot directly prove pixel output

The renderer should keep evolving toward a stronger world-aware rendering system, but it no longer needs to be described as a pre-foundation “test scene” renderer.

### 5.4.1 Current Runtime UI Direction

Carrot now has a real first-pass in-game UI foundation:

* retained widget tree ownership
* layout primitives
* focus and directional navigation
* UI input ownership policy
* renderer-stage integration

The next UI priority is not a giant widget explosion.

The current UI direction is:

* keep the existing text, layout, and focus/navigation foundations usable and understandable
* add only the next genuinely needed widgets and diagnostics surfaces
* use runtime UI to validate engine iteration/tooling workflows before growing a broader editor UI stack

The sandbox should validate this work, but the implementation should remain primarily engine-owned.

### 5.5 Physics / Collision Direction

Carrot's intended physics direction is **gameplay-first collision and movement infrastructure**, not a large simulation-first engine stack.

Near-term priorities are:

* collision queries
* tilemap collision
* Tiled-authored object collision
* trigger volumes
* kinematic movement constraints

Longer-term work can later expand into:

* specialized movement motors
* support-based grounded traversal for HD2D topography
* lightweight rigid bodies where useful

Gravity should be treated as a property of active movement/body mode, not a universal assumption for every actor or game style.

Current implemented first pass:

* `collision_world_t` with filterable static collision queries
* Tiled tileset rectangle collision imported into runtime static blocking
* top-down kinematic player movement constrained by authored collision
* trigger rectangles imported from Tiled object layers
* gameplay-facing trigger enter / exit events
* toggleable collision debug visualization for map collision and object colliders

### 5.6 Asset System

Carrot’s asset system is intended to provide a clean separation between:

* source files
* authored asset metadata
* runtime loaded assets
* future optional cooked / cached artifacts

The asset system is a major pillar of the engine and should remain explicit and tool-friendly.

### 5.7 Audio Engine

Carrot includes a custom audio engine rather than treating audio as an afterthought.

The audio system is intended to be a serious first-class subsystem, not just a thin playback wrapper.

### 5.8 Future World / Object Layer

Carrot is intended to grow into a world-driven architecture over time.

That direction now also includes a gameplay-facing controller layer:

* default engine controllers should provide common reusable mechanics
* games should configure or override those controllers rather than reinventing them each time
* the engine should not hardcode one game’s meanings, authored ids, or content rules

That includes future systems such as:

* world state
* `world_object_t` lifetime and ownership
* gameplay-facing simulation systems
* rendering and audio integration driven from world state
* scene/world organization
* modular object functionality where appropriate

This layer should be introduced when the engine is ready for it, not before.

Carrot is not currently intended to force a pure ECS architecture across the entire engine.

Instead, the long-term direction favors a model built around:
* meaningful `world_object_t` instances with clear identity
* reusable `component_t` functionality where it genuinely helps
* explicit ownership
* data-oriented implementation where performance matters

Not every `world_object_t` is expected to be spatial.

That means spatial state such as transforms should remain explicit through components rather than being forced into the base world object type by default.

The goal is to preserve both clarity and performance without forcing the engine into an architectural model that does not fit its intended design.

Current practical progress already includes:

* tilemap-backed world import through authored Tiled data
* default movement and proximity-interaction controllers with game-side semantic handling
* authored `*.scene.json` scene assets for initial world bootstrap
* scene transitions driven by authored door properties and spawn markers
* engine-level input action mapping above raw key codes
* a small automated test target covering scene discovery, import, and load behavior

---

## 6. Rendering & RHI Direction

Carrot’s rendering architecture is intentionally split into:

* **platform windowing**
* **RHI backend implementation**
* **renderer logic**
* **higher-level draw systems**

This separation is important.

### Current Rendering Direction

Carrot is currently centered around a minimal but meaningful rendering path:

* textured quad rendering
* backend bring-up
* batching
* shader loading
* resource binding
* frame lifecycle flow

This work is foundational and intentionally precedes more advanced gameplay-facing rendering systems.

### Backend Direction

The goal is to maintain a shared engine-facing rendering path while supporting multiple native graphics APIs behind the scenes.

Current backend direction:

* **Vulkan** as a strong baseline backend
* **Metal** for native Apple support
* **DirectX 12** for native Windows support

Parity between these backends is an important near-term goal.

### Shader Direction

Carrot’s long-term shader policy is to maintain a **single engine-facing shader source set** rather than separate hand-authored shaders per backend.

The canonical shader language for Carrot is **HLSL**.

From this shared source, the build pipeline produces backend-specific shader outputs for:

- **Vulkan**
- **Metal**
- **DirectX 12**

Backend-specific shader binaries and translated outputs are treated as **derived artifacts**, not primary authored sources.

This policy exists to:

- eliminate duplicated shader logic across backends
- keep rendering behavior consistent across APIs
- reduce long-term maintenance complexity
- support a coherent multi-backend rendering architecture

---

## 7. Asset System Direction

The asset system is one of Carrot’s most important long-term systems.

### Core Asset Philosophy

Carrot assets should be explicit and understandable.

The engine should clearly distinguish between:

* **source media**
  such as `.png`, `.wav`, shader source files, etc.

* **authored asset definitions**
  such as `*.texture.json`, `*.audio.json`, and future similar asset descriptors

* **runtime loaded assets**
  such as decoded images, audio samples, GPU textures, and streaming objects

* **imported / cooked / cached artifacts**
  engine-native derived data such as `.cfont`, `.ctex`, `.caud`, `.csprite`, and `.cmap`

### Virtual File System

Carrot uses a virtual file system to keep asset access portable and explicit.

Current virtual roots include:

* `engine://`
* `game://`
* `source://`
* `save://`

This allows engine and game content to remain clearly separated and relocatable.

### Tool-Friendly Asset Philosophy

Carrot is intended to work well with authored metadata and external content tools rather than requiring a fully custom editor from the beginning.

This is one reason JSON is currently a strong fit for authored asset definitions.

### External Tool Workflows

Carrot supports established external tools as first-class content workflows where they make sense.

Current engine-facing integrations include:

* **Aseprite**
  for sprite sheets, animation data, and frame/tag-based workflows

* **Tiled**
  for tile maps, object layers, animated tiles, and larger multi-map / streaming-style world layouts

The goal is not to replace these tools immediately, but to integrate with them cleanly and intentionally.

---

## 8. Audio System Direction

Carrot’s audio engine is intended to be a serious subsystem with clean architecture and long-term utility.

### Audio Philosophy

Audio should not be treated as a thin “play sound file” layer.

Instead, Carrot’s audio architecture is intended to support:

* reliable playback
* low-latency mixing
* streaming and sample playback
* bus-based routing
* DSP processing
* future gameplay-facing integration

### Current Direction

Carrot’s audio system is being built around:

* a fixed internal mix model
* a custom mixer
* audio buses
* voice control
* sample and streaming playback paths
* backend abstraction for platform audio devices

### Long-Term Direction

Over time, the audio system should be able to support:

* richer in-game audio workflows
* stronger asset/tool integration
* engine-native authoring support where appropriate
* better diagnostics and inspection surfaces

Carrot’s audio engine is intended to remain a real strength of the project.

---

## 9. World / Object Architecture Direction

Carrot is intended to eventually include a **world-driven object architecture** that can support gameplay-facing simulation, rendering, audio integration, and broader game structure.

This is an important long-term direction for the engine and should be preserved explicitly.

### World / Object Philosophy

When introduced, this layer should aim to be:

* explicit
* performant
* understandable
* well-integrated with rendering, audio, and gameplay systems
* flexible enough to support both strong object identity and reusable modular behavior

The intended design direction favors:

* meaningful `world_object_t` instances with clear identity
* `component_t` functionality where it provides real value
* engine-owned world state and update flow
* practical, performance-conscious implementation
* minimal unnecessary runtime overhead

Carrot is not currently intended to be a pure ECS-first engine.

Instead, the goal is to preserve a balance between:

* object-oriented gameplay structure
* modular composition where appropriate
* data-oriented implementation in hot paths where it improves performance

That means Carrot should still care deeply about:

* efficient storage
* avoiding unnecessary pointer chasing
* minimizing cache-unfriendly behavior
* tight iteration in performance-critical systems
* clear ownership and predictable runtime behavior

Not every `world_object_t` is expected to carry spatial data by default.

For example, many world-facing characters, NPCs, props, interactables, and renderable objects will likely use a `transform_component_t`, but some world-level controllers or manager-style objects may not.

That distinction is intentional.

### Timing

This layer should not be forced into the engine prematurely.

It belongs at the point where Carrot transitions from:

* manually driven test rendering and isolated systems

into:

* world-driven rendering
* gameplay-facing simulation
* `world_object_t`-based scene/world organization

That means this layer is expected to arrive alongside the engine’s **simple world / scene rendering flow**, not before.

---

## 10. Development Environment

Carrot is currently developed with a practical, cross-platform desktop workflow.

### Primary Development Tools

* **CMake**
* **CLion**
* **clang / AppleClang**
* **Git submodules** for selected dependencies

### Compiler Strategy

For now, Carrot intentionally prioritizes **clang-family compiler support**.

This is partly due to:

* current low-level engine implementation choices
* CarrotHLM’s current compiler assumptions
* the desire to minimize compiler-related variance while the engine is still maturing

Broader compiler support may happen later, but it is not a near-term priority.

### Documentation Separation

Project rules and implementation guidance are intentionally split across multiple docs.

This file is the high-level direction document.

See also:

* `README.md`
* `CODING_STANDARDS.md`
* `ARCHITECTURE_NOTES.md`

---

## 11. Development Priorities

Carrot’s roadmap is best understood in terms of **capabilities**, not fake calendar certainty.

### 11.1 Current Focus Areas

These are the current engine priorities after the completed foundations in Milestones 01 through 12.

* runtime iteration, reload, and diagnostics
* a thin optional tooling/editor host built on top of engine APIs rather than inside them
* sharper asset-pipeline visibility around invalidation, regeneration, and cache state
* scene/runtime polish and broader gameplay-facing runtime APIs
* preserving renderer backend parity as real-world renderer features broaden

Recent completed foundation work now includes:

* explicit frame stages for world, UI, composite, overlay debug, and log-console rendering
* world-aware scene rendering and authored scene transitions
* collision queries, authored blocking collision, and trigger import
* layered 2D rendering and visibility-zone behavior
* controller/device support and action-map-based gameplay input
* in-game UI foundation and runtime text rendering
* richer Tiled authored-data support
* imported/cooked asset artifacts for fonts, textures, audio, sprites, and tilemaps
* user-facing input rebinding, persisted bindings, and first-pass broader gameplay input routing

### 11.2 Next-Tier Growth

These are important next-tier areas, but they should advance because they solve real workflow or gameplay structure problems, not because they have lingered on an old checklist.

* deeper diagnostics and polish on the new input routing/rebinding foundation
* screen-transition presentation modules built on top of the stronger frame architecture
* save / persistence architecture
* broader gameplay/runtime cleanup where stable engine patterns should move out of sandbox code
* deeper runtime and tooling inspection surfaces for assets, world state, and diagnostics

For current input-router direction, see [input_router_direction.md](/Users/zshrout/dev/CarrotGameEngine/docs/systems/input_router_direction.md).

Current input-system boundary after Milestone 15:

* player 0 remains the current primary UI/navigation/debug-input owner
* fixed assignment exists as the first broader routing mode
* rebinding currently edits shared action bindings rather than divergent per-player binding profiles
* join-flow and per-player binding divergence remain future work

### 11.3 Longer-Horizon Work

These remain meaningful future directions, but they are not current priorities simply because they are desirable someday.

* richer world/object architecture growth where the current scene/world model stops being enough
* better asset authoring workflows where they provide clear value beyond external tools
* 2D lighting and later shadow support
* hybrid 2D/3D rendering expansion
* animated 3D model support

Long-term work should remain grounded in Carrot’s actual needs rather than feature-chasing.

---

## 12. What Carrot Should Avoid

It is just as important to preserve what Carrot should **not** become.

Carrot should avoid:

* chasing feature parity with giant engines for its own sake
* introducing systems before the engine is ready for them
* over-abstracting every problem prematurely
* depending on large runtime frameworks unnecessarily
* sacrificing clarity for “cleverness”
* bloating into an engine that is harder to understand than it is to use

Carrot should remain:

* explicit
* coherent
* intentional
* technically strong
* enjoyable to build with

That matters more than raw feature count.

---

## 13. Closing Direction

Carrot is intended to become a long-term engine foundation with:

* strong native platform support
* a clean rendering architecture
* a serious audio engine
* a practical asset pipeline
* room for future world / object systems
* room for future tooling without being dependent on it

It should grow carefully and deliberately.

The goal is not to build the biggest engine.

The goal is to build an engine that is **worth building games in**.
