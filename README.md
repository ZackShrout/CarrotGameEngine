# Carrot Game Engine 🥕

<p align="center">
  <img src="assets/images/carrot_engine_logo_512.png" alt="Carrot Engine Logo" width="220">
</p>

<p align="center">
  <strong>Native cross-platform C++23 game engine by BunnySoft.</strong>
</p>

Carrot is a custom engine focused on **native desktop development**, **explicit control**, and **clean architecture**. It is being built primarily for the kinds of games I actually want to make — especially **2D**, **hybrid 2D/3D**, and **RPG-style** projects — rather than as a general-purpose “do everything for everyone” engine.

It is currently under active development and is evolving rapidly.

---

## Current Status

Carrot is in **early but very real** development.

The engine already includes meaningful progress in several major areas:

* Cross-platform native windowing
* Multi-backend rendering architecture
* Vulkan backend functional and actively used as the baseline renderer
* Metal textured quad renderer at practical parity with Vulkan
* DirectX 12 textured quad renderer at practical parity with Vulkan and Metal
* Custom audio engine architecture
* Virtual file system and asset pipeline foundations
* Texture and audio asset loading
* Shader and asset hot-reload direction
* CMake-based cross-platform build system

Carrot is **not production-complete**, but it is also **not a toy repo**. The goal is to build a focused, long-lived engine foundation.

---

## Core Goals

Carrot is being built around a few non-negotiable ideas:

* **Native first**
  No SDL, no GLFW, no giant runtime framework abstraction sitting between the engine and the OS.

* **Cross-platform desktop support**
  Windows, Linux, and macOS are all first-class targets.

* **Modern C++23**
  Strong type safety, explicit ownership, and clean low-level control.

* **Focused engine design**
  Carrot is not trying to become Unreal or Unity. It is being built to serve a specific style of game development well.

* **Explicit systems over magic**
  Rendering, audio, assets, and engine systems should be understandable and debuggable.

* **Tool-friendly asset pipeline**
  Carrot is designed to work well with authored metadata and established external tools rather than forcing everything through a custom editor from day one.

---

## Platforms & Graphics APIs

### Platforms

* **Windows**
* **Linux**
* **macOS**

### Windowing Backends

* **Win32** (Windows)
* **Wayland** (Linux primary)
* **Cocoa** (macOS)
* **X11** (planned Linux fallback backend)

### Graphics Backends

* **Vulkan**
* **Metal**
* **DirectX 12**

Carrot’s rendering architecture is built around a custom **RHI (Rendering Hardware Interface)** so higher-level engine systems are not tied directly to one graphics API.

---

## Planned Content Tooling Direction

Carrot is intended to work well with established external content tools rather than requiring a custom editor immediately.

Planned first-class content workflows include:

* **Aseprite** for sprite sheets, frame-based animation, and animation metadata
* **Tiled** for tile maps, object layers, animated tiles, and larger world/map composition workflows

This is one of the reasons Carrot leans heavily into **JSON-based authored metadata** for assets and import definitions.

---

## Major Systems (In Progress)

### Rendering

* Cross-platform rendering abstraction (RHI)
* Textured quad rendering
* Practical textured quad parity between Vulkan / Metal / DirectX 12
* Shared HLSL shader source pipeline across Vulkan / Metal / DirectX 12
* 2D camera / projection support with both:
  * responsive world-view framing
  * fixed-aspect letterboxed gameplay framing
* Early 2D sprite-facing renderer features including:
  * explicit draw layering and intra-layer ordering
  * frame/default pivot support with per-draw pivot override
  * sprite flip X / flip Y support
* Tilemap Foundation V1 including:
  * Tiled-backed tilemap asset import
  * Carrot runtime tilemap structures preserving tile layers, object layers, and tilesets
  * first orthogonal tile-layer rendering through the renderer
  * visible Tiled tile-object rendering for authored `props` layers
  * runtime object-layer queries for named markers and semantic object types
  * early hybrid object support using Tiled object `type` plus typed custom properties
* Engine-owned debug text / overlay support for runtime renderer diagnostics
* Future direction includes:

    * animation
    * UI layers

### Audio

* Custom audio engine
* Fixed internal mix rate
* Bus-based mixing architecture
* Streaming and sample playback support
* DSP-oriented system design
* Cross-platform backend support

### Assets

* Virtual file system with paths such as:

    * `engine://`
    * `game://`
    * `source://`
    * `save://`
* Authored metadata assets such as:

    * `*.audio.json`
    * `*.texture.json`
* Importer / loader split
* Clear separation between:

    * source files
    * asset definitions
    * runtime loaded assets

### ECS / World Systems

Carrot is intended to grow into a custom **ECS / world-driven architecture**, but that system is being introduced intentionally and at the right time rather than forced in before the engine actually needs it.

---

## Build System

Carrot currently builds with:

* **CMake 4.1+**
* **C++23**
* **clang / AppleClang**
* **CLion** as the primary development environment

> **Current compiler support note:**
> Carrot is currently developed and tested with **clang-family compilers only**.
> **GCC**, **MSVC**, and other toolchains should **not** currently be assumed to work.

This is intentional for now. CarrotHLM and parts of the current low-level setup are written around the clang toolchain, and the project currently prioritizes consistency across platforms over broad compiler support.

The project is primarily developed in **CLion**.

CMake project generation for other environments may be possible, including **Xcode** and **Visual Studio**, but the supported toolchain is currently still the **clang family**, not arbitrary host-default compilers.

---

## Tested Environments

Carrot is currently being developed and actively tested with:

* **macOS + Apple Clang**
* **Linux + clang**
* **Windows + clang-cl**

Other compiler / platform combinations may eventually be supported, but should not currently be assumed to work.

---

## Repository Layout

```text
CarrotGameEngine/
├── CMakeLists.txt
├── CompileShaders.cmake
├── README.md
├── docs/
├── assets/
├── deps/
├── shaders/
├── src/
│   ├── Engine/
│   └── Game/
└── tools/
```

> The exact internal structure will continue to evolve as the engine matures.

---

# Building Carrot

## 1) Clone the repository

```bash
git clone --recurse-submodules git@github.com:ZackShrout/CarrotGameEngine.git
cd CarrotGameEngine
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

---

## 2) Make sure you have the required tools

### Required Everywhere

* **CMake 4.1+**
* **Git**
* **clang / AppleClang**
* **A C++23-capable standard library**
* **Ninja** *(recommended)*

> Carrot is currently expected to be built with **clang-family compilers**.
> Builds with **GCC**, **MSVC**, or other toolchains are currently unverified and unsupported.

---

# Platform-Specific Requirements

## Windows

### Current Status

* Intended compiler: **clang-cl**
* CMake may generate Visual Studio solutions, but **MSVC should not currently be assumed to work**

### Recommended

* **Visual Studio 2022 Build Tools**
* **LLVM / clang-cl**
* **CMake**
* **Git**
* **Ninja** *(recommended)*

### Additional Requirements

* **Windows 10/11 SDK**
* **Vulkan SDK** *(recommended for Vulkan development)*

### Notes

* DirectX 12 support uses the Windows SDK
* Vulkan development may require the LunarG Vulkan SDK for headers, validation layers, and tooling

---

## macOS

### Current Status

* Intended compiler: **Apple Clang**

### Required

* **Xcode Command Line Tools**
* **CMake**
* **Git**

Install Xcode command line tools:

```bash
xcode-select --install
```

### Notes

* Metal development requires Apple platform tooling
* Full Xcode is useful for debugging and platform-specific troubleshooting

---

## Linux

### Current Status

* Intended compiler: **clang**
* Current Linux builds also expect **libc++**
* GCC should **not** currently be assumed to work

### Required

* **CMake**
* **Git**
* **clang**
* **pkg-config**
* **Ninja** *(recommended)*

### Likely Required Development Packages

On Debian / Ubuntu-style systems, expect to need packages along these lines:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    clang \
    cmake \
    ninja-build \
    pkg-config \
    libc++-dev \
    libc++abi-dev \
    wayland-protocols \
    libwayland-dev \
    libxkbcommon-dev \
    libvulkan-dev \
    vulkan-validationlayers-dev \
    libasound2-dev \
    libpulse-dev
```

### Notes

* Carrot currently targets **Wayland** as the primary Linux windowing backend
* **X11 support is planned**, but should not currently be assumed as the primary path
* Vulkan development on Linux generally requires:

    * Vulkan headers / loader
    * validation layers
    * appropriate GPU drivers

> Depending on distro and GPU vendor, package names may differ slightly.

---

# Configuring & Building

## Recommended out-of-source build

```bash
cmake -S . -B build
cmake --build build
```

### Debug build example

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Release build example

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Running

Once built, the primary sandbox / game executable should appear in your build output directory.

Typical examples:

```bash
./build/bin/Debug/CarrotSandbox
```

or on macOS / Windows depending on generator and configuration.

> Exact output paths may vary slightly depending on platform and generator.

---

# Dependencies

Carrot intentionally prefers either:

* **system libraries**, or
* **statically linked / source-controlled dependencies**

The goal is to avoid runtime dependency chaos.

Current / planned dependency categories include:

* **Vulkan SDK / system Vulkan headers**
* **Wayland / Linux platform development headers**
* **CarrotHLM** (custom math library)
* **CarrotPNG** (custom PNG loader)
* optional development / profiling / shader tooling where appropriate

This project is intentionally opinionated about keeping the runtime environment lean.

---

## Documentation

If you're trying to understand Carrot quickly:

- `CARROT_MASTER_PLAN.md`  
  High-level direction, philosophy, and roadmap

- `ARCHITECTURE_NOTES.md`  
  System-level architecture and layering

- `CODING_STANDARDS.md`  
  Code style, conventions, and structure rules

Subsystem documentation lives under `docs/`:

- `docs/systems/asset_pipeline.md`
- `docs/systems/audio_engine.md`
- `docs/systems/json_spec.md`

Schema and format references:

- `docs/schemas/audio_asset_json_schema.md`

---

# Long-Term Direction

Carrot is being built to support:

* native desktop game development
* strong 2D and hybrid 2D/3D workflows
* sprite animation and tile-based world pipelines
* robust audio systems
* explicit, understandable engine architecture
* long-term maintainability

The goal is not to become the biggest engine.

The goal is to become a **great engine for the games it is meant to build**.

---

# License

License details for Carrot will be finalized later.

For now, unless otherwise stated, this project should be considered **All Rights Reserved**.
