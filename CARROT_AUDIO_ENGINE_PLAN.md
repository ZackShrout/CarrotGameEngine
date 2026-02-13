# Audio Engine – Master Plan

> A designer-first, engine-grade audio system built to scale from 2D to full 3D, with clean intent-based APIs and a future-ready editor pipeline.

---

## 1. Vision & Principles

### Goals

* **Laughably easy to use** for gameplay code
* **Hard to misuse** by construction
* **Designer-centric** (mixing desk mental model)
* **RT-safe** and backend-agnostic
* **Editor-ready** without refactors

### Non-Goals (for now)

* Recording-console DSP chains (EQ/compression per strip)
* Front/back localization (HRTF)
* Full middleware UI

---

## 2. Mental Model (Mixing Desk)

* **Sound Asset** → Channel strip preset
* **Bus** → Subgroup (music / sfx / ui / ambience)
* **Play Call** → Patching signal into the desk
* **Spatial Mode** → Mic placement
* **Handle** → Hand on a fader (only when needed)

---

## 3. Core Vocabulary

### Sound Assets

* Own defaults (bus, gain, distances, looping capability)
* Gameplay code does *not* restate defaults
* Future: variations, random pitch/gain, tags

### Buses

```cpp
enum class audio_bus : uint8_t {
    master,
    music,
    sfx,
    ui,
    ambience,
};
```

### Spatial Intent

```cpp
enum class spatial {
    none,     // UI, music
    planar,   // 2D / HD2D
    full_3d   // future HRTF
};
```

### Audio Handle

```cpp
struct audio_handle { uint32_t id{0}; };
```

* Returned only for long-lived / looping sounds
* Fire-and-forget returns no handle

---

## 4. Public API Layers

### Layer 1 – Joyful Statics (90% Case)

```cpp
namespace audio {
void play_2d(sound_id sound) noexcept;
void play_planar(sound_id sound, float x, float y) noexcept;
void play_3d(sound_id sound, const chlm::float3& pos) noexcept;
}
```

* Zero configuration
* Asset-driven defaults
* Impossible to misuse

### Layer 2 – Descriptor-Based Power

#### Planar Descriptor

```cpp
struct play_planar_desc {
    chlm::float2 position{0.f, 0.f};
    float gain{1.f};
    float ref_distance{1.f};
    float max_distance{30.f};
    audio_bus bus_override{audio_bus::sfx};
};
```

```cpp
void play_planar(sound_id sound, const play_planar_desc& desc) noexcept;
```

#### 3D Descriptor (Future-Safe)

```cpp
struct play_3d_desc {
    chlm::float3 position{0.f, 0.f, 0.f};
    float gain{1.f};
    float ref_distance{1.f};
    float max_distance{50.f};
    audio_bus bus_override{audio_bus::sfx};
};
```

---

## 5. Long-Lived & Looping Sounds

```cpp
audio_handle play_looping_planar(sound_id sound,
                                const play_planar_desc& desc) noexcept;

void stop(audio_handle h, float fade_out_seconds = 0.f) noexcept;
void set_position(audio_handle h, float x, float y) noexcept;
void set_gain(audio_handle h, float gain) noexcept;
```

Rules:

* Handles are rare and deliberate
* No runtime changes to bus or spatial mode

---

## 6. Spatialization Roadmap

### Implemented

* Manual pan pots
* Distance attenuation
* Planar pan (X-axis)

### Planned

* Listener orientation (data present)
* Orientation-aware planar pan
* Full 3D via HRTF (future)

---

## 7. Listener Model

```cpp
struct audio_listener_t {
    chlm::float3 position{0.f, 0.f, 0.f};
    chlm::float3 forward {0.f, 1.f, 0.f};
};
```

* Data-first; behavior added later
* Aligned with engine math conventions

---

## 8. Engine Architecture Constraints

* Audio backend owns the RT thread
* Engine communicates via lock-free commands
* No allocations or locks in render callback
* Backends may differ per platform

---

## 9. Editor & Middleware Alignment

* Descriptor structs are editor nodes-in-waiting
* Runtime API is a strict subset of editor features
* Assets define defaults; code overrides are exceptions

---

## 10. Next Steps

1. Define **Sound Asset** structure & defaults
2. Decide asset loading & lifetime
3. Implement looping behavior & fades
4. Smarter voice stealing & prioritization 
5. Variation & randomization
6. Listener orientation usage

---

> This document is a compass. If a feature doesn’t align with it, pause and reassess.
