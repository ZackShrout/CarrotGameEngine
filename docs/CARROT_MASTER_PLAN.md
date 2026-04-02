# Carrot Game Engine – Master Plan

**BunnySoft**
**Version 2.0**
**Last Updated: March 2026**

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

Current / planned platform windowing support:

* **Win32** on Windows
* **Wayland** on Linux (primary)
* **Cocoa** on macOS
* **X11** on Linux (planned fallback / additional backend)

Wayland is currently the preferred Linux path, but X11 is still considered an important long-term compatibility target and is intended to be supported as the engine matures.

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
* Wayland / future X11 Linux windowing
* Cocoa windowing
* platform-specific file and executable path utilities

### 5.2 Core Systems

Core systems provide the foundational engine infrastructure.

These include:

* engine lifecycle / module ownership
* logging
* config loading
* event dispatch
* delegates / callbacks
* memory utilities
* file utilities
* debug helpers
* hot-reload support

These systems should remain lightweight, dependable, and broadly reusable across the engine.

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

Near-term renderer priorities include:

* textured quad rendering
* batching
* backend parity
* camera / projection support
* gameplay-facing presentation policies such as fixed-aspect letterboxing
* sprite rendering
* stable layered 2D draw ordering
* sprite placement controls such as pivot/origin and flip
* debug rendering and text
* engine-owned diagnostics overlays built on top of the existing 2D renderer

Over time, the renderer should evolve from “test scene rendering” into a world-aware rendering system that can support actual gameplay-driven rendering flows.

### 5.5 Asset System

Carrot’s asset system is intended to provide a clean separation between:

* source files
* authored asset metadata
* runtime loaded assets
* future optional cooked / cached artifacts

The asset system is a major pillar of the engine and should remain explicit and tool-friendly.

### 5.6 Audio Engine

Carrot includes a custom audio engine rather than treating audio as an afterthought.

The audio system is intended to be a serious first-class subsystem, not just a thin playback wrapper.

### 5.7 Future World / Object Layer

Carrot is intended to grow into a world-driven architecture over time.

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

* **future cooked or cached artifacts**
  optional engine-native derived data such as texture/audio caches if and when they become worthwhile

### Virtual File System

Carrot uses a virtual file system to keep asset access portable and explicit.

Planned / current virtual roots include:

* `engine://`
* `game://`
* `source://`
* `save://`

This allows engine and game content to remain clearly separated and relocatable.

### Tool-Friendly Asset Philosophy

Carrot is intended to work well with authored metadata and external content tools rather than requiring a fully custom editor from the beginning.

This is one reason JSON is currently a strong fit for authored asset definitions.

### Planned External Tool Workflows

Carrot is intended to support established external tools as first-class content workflows where they make sense.

Important planned integrations include:

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
* future middleware/editor-style possibilities if they ever become worth pursuing

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

### 11.1 Near-Term Priorities

These are the most immediate engine priorities.

* Preserve and verify **Vulkan**, **Metal**, and **DirectX 12** textured quad parity as renderer features expand
* Continue stabilizing the renderer / RHI boundary
* Expand textured quad rendering into a stronger general 2D rendering foundation
* Introduce **sprite asset support**
* Support **sprite animation workflows**
* Continue expanding **camera / projection support** into stronger gameplay/editor camera workflows
* Continue expanding sprite-facing renderer behavior such as layered draw ordering, pivot/origin handling, and flip support
* Continue expanding Tiled-backed tilemap support from the current foundation into stronger gameplay/world rendering behavior
* Continue formalizing Tiled object-layer conventions for markers, props, and hybrid objects
* Continue moving rendering from ad hoc test scenes into world-aware scene/object rendering paths
* Continue improving **debug rendering / debug text / overlay direction**, beginning from the current engine-owned text overlay path
* Continue strengthening the asset system and authored asset workflows

### 11.2 Mid-Term Priorities

These priorities build on the near-term rendering and asset foundation.

* Introduce a **simple world / scene rendering flow**
* Introduce the first meaningful version of **world-driven object architecture**
* Expand rendering from test-scene logic into world-driven rendering
* Add stronger support for:

    * sprites
    * tile maps
    * layered rendering
    * world-space 2D workflows
* Improve input routing / layer-aware input handling
* Continue maturing asset import and runtime asset systems

### 11.3 Long-Term Priorities

These are meaningful future directions, but should only be pursued on top of strong foundations.

* Early tooling / editor beginnings
* Better asset authoring workflows
* Save / persistence architecture
* World and gameplay tooling improvements
* Broader game-facing engine systems
* Possible future engine-native tools where they genuinely add value

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
