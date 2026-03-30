# Carrot Game Engine – Audio Engine

**BunnySoft**
**System Design Document**
**Last Updated: March 2026**

---

## 1. Purpose

Carrot’s audio engine is a custom, engine-native audio system designed to provide:

* clean gameplay-facing audio control
* strong runtime behavior
* real-time-safe architecture
* bus-based mixing and processing
* room for future tooling and authoring workflows

Audio in Carrot is intended to be a **first-class engine subsystem**, not just a thin “play sound file” wrapper.

This document describes how the audio engine is structured, what its current design goals are, and how it is intended to evolve.

It should be understandable to:

* engine contributors
* engine users
* future tooling/editor work
* AI/code assistants interacting with the codebase

This is not intended to be a low-level implementation spec.
It is the architectural model for how Carrot audio is intended to work.

---

## 2. Core Goals

Carrot’s audio engine is built around a few important goals.

### 2.1 Easy to Use Correctly

Gameplay-facing audio should be straightforward to use without requiring every call site to manually restate engine defaults.

The engine should make common audio usage simple and obvious.

### 2.2 Hard to Misuse

The audio engine should guide callers toward sensible usage patterns and avoid exposing unnecessarily error-prone control surfaces everywhere.

### 2.3 Real-Time Safe by Design

The audio callback / render path must remain safe and predictable.

That means:

* no allocations in hot callback paths
* no blocking or locks in the render callback
* explicit control over runtime ownership and data flow

### 2.4 Strong Internal Architecture

The audio engine should remain understandable as it grows.

Playback, routing, DSP, streaming, asset integration, and platform backends should remain clearly structured rather than collapsing into one giant “audio system blob.”

### 2.5 Future-Ready Without Premature Bloat

Carrot audio should leave room for:

* richer gameplay integration
* stronger authoring workflows
* future tooling/editor possibilities

…without trying to become a giant middleware clone too early.

---

## 3. Audio Philosophy

Carrot’s audio engine is intentionally designed around a practical mental model:

> **sound assets define defaults**
> **gameplay code expresses intent**
> **the engine handles playback, routing, and processing**

This is important.

Gameplay code should usually be saying things like:

* play this sound
* play this music
* play this sound here
* loop this sound
* stop this voice
* move this emitter

…not re-describing every detail of how the sound should fundamentally behave every time.

That means Carrot audio is intended to be:

* asset-driven
* bus-aware
* command-driven
* runtime-safe
* future tool-friendly

---

## 4. High-Level Architecture

At a high level, Carrot’s audio engine is organized around:

* **audio assets**
* **voices**
* **commands**
* **mixer / buses**
* **DSP**
* **audio clock**
* **platform backends**

These pieces should remain distinct.

---

## 5. Audio Assets

Audio assets define the authored defaults for a sound or music resource.

They are the bridge between the asset pipeline and the runtime audio engine.

Examples of authored audio metadata include:

* asset ID
* source file
* bus
* gain
* looping
* spatial behavior
* streaming behavior

### Why This Matters

The audio engine should not require gameplay code to manually restate:

* default bus
* whether something loops
* whether something should stream
* whether it is spatialized
* its default playback characteristics

That belongs to the authored asset.

### Runtime Role

At runtime, audio assets become stable engine-facing structures that playback systems can use efficiently and predictably.

---

## 6. Voices

A **voice** is an active playback instance in the audio engine.

Examples:

* one currently playing sound effect
* one currently playing music stream
* one looping ambient emitter
* one currently active UI sound

A voice is not just the asset itself.

It is a **runtime playback instance** of an asset.

### Voice Responsibilities

A voice is responsible for tracking things such as:

* playback position
* gain / pan / spatial parameters
* loop state
* envelope state
* whether it is a sample-backed or stream-backed voice
* whether it is active / paused / stopping / finished

### Voice Types

Carrot currently supports two important playback models:

#### Sample Voices

These play from fully available decoded sample data already resident in memory.

These are appropriate for:

* sound effects
* UI sounds
* short repeated sounds
* low-latency playback cases

#### Stream Voices

These play from streamed data rather than requiring the full decoded file to be resident in memory at once.

These are appropriate for:

* music
* long ambience
* larger long-form audio content

This split is important and should remain explicit in the engine.

---

## 7. Fixed Internal Mix Rate

Carrot mixes internally at a **fixed 48 kHz**.

This is an intentional architectural choice.

### Why 48 kHz

Carrot standardizes internally on 48 kHz because it provides:

* a clean engine-wide audio target
* predictable DSP behavior
* consistent playback and timing assumptions
* simpler internal mixing behavior
* easier cross-platform reasoning

### What This Means

Audio content may originate at other sample rates.

That is acceptable.

Carrot handles this by resampling as needed so that the internal engine mix model remains fixed.

### Current Policy

* memory-resident samples are resampled to the engine mix rate during load
* streamed audio is resampled into the streaming path as needed
* backend device mismatch is handled separately from the internal engine mix model

This is a strong and intentional design decision and should remain part of the engine’s audio identity.

---

## 8. Mixer and Buses

Carrot’s audio engine is built around a **bus-based mixer**.

This gives the engine a much stronger and more scalable audio model than flat one-off playback.

### Why Buses Exist

Buses provide:

* logical grouping
* shared gain control
* routing structure
* DSP insertion points
* future tooling/editor compatibility

### Typical Bus Categories

Examples include:

* `master`
* `music`
* `sfx`
* `ui`
* other future content-specific buses as needed

### Design Intent

A sound asset should generally know what bus it belongs to by default.

That means gameplay code usually should not need to say:

> “this footstep is a sound effect and belongs on the SFX bus”

…every time it plays.

That belongs in the authored asset model.

---

## 9. DSP Direction

Carrot’s audio engine is intended to support meaningful DSP processing as part of the mixer architecture.

This is already part of the engine’s design identity.

### Current / Intended DSP Role

DSP in Carrot exists to support:

* practical mix shaping
* routing-based effects
* engine-native audio polish
* future stronger content workflows

### Architectural Role

DSP should live as part of the audio system’s structured routing/mixing architecture, not as random ad hoc effect code scattered through playback logic.

### Current DSP Direction

Carrot’s audio engine is designed to support and/or already includes systems such as:

* filtering
* delay
* reverb
* saturation
* compression
* limiting
* bus-level processing

This is an area where Carrot’s audio engine is intended to remain stronger and more intentional than a minimal “just make sounds happen” implementation.

---

## 10. Audio Clock and Timing

Carrot maintains an internal **audio clock** based on audio frame progression.

This exists because the audio engine should have its own coherent sense of time rather than relying on vague external timing assumptions.

### Why This Matters

A real audio clock helps support:

* playback timing
* streaming state progression
* envelope progression
* synchronization-sensitive systems
* future gameplay/audio integration

The audio engine should be able to reason about audio progression in audio terms, not only in frame-update terms.

---

## 11. Command-Driven Runtime Model

Carrot’s audio engine is intended to be **command-driven**.

That means gameplay and engine systems communicate desired audio changes into the audio engine, rather than directly mutating callback-owned state from arbitrary threads.

This is a major architectural rule.

### Why Commands Exist

Commands help preserve:

* real-time safety
* thread separation
* predictable ownership
* backend independence
* cleaner gameplay-facing APIs

### Typical Command Examples

Examples include:

* play sound
* play stream
* stop voice
* pause voice
* resume voice
* stop all
* set voice parameters
* set bus parameters

### Design Intent

The audio engine should generally be fed **intent** from the rest of the engine.

The audio thread / render path then consumes and applies that intent safely.

This is a very important boundary and should remain intact.

---

## 12. Real-Time Safety Constraints

The audio render callback is one of the most sensitive execution environments in the engine.

This means Carrot’s audio architecture must continue respecting a few strict rules.

### The Render Path Should Avoid:

* dynamic allocation
* blocking operations
* locks / mutex waits
* filesystem access
* arbitrary complex authoring logic
* anything that could unpredictably stall audio output

### The Render Path Should Prefer:

* stable preallocated data
* bounded work
* explicit queues / ring buffers
* predictable voice/mixer traversal
* runtime-safe data structures

These are not “nice to have” rules.
They are foundational to a stable engine-native audio system.

---

## 13. Streaming Architecture

Streaming is a first-class part of Carrot’s audio model.

It is not a bolt-on special case.

### Why Streaming Matters

Some audio content should not require fully decoding and holding the entire source in memory.

Examples:

* music
* longer ambience
* longer-form environmental audio

### Design Intent

Streaming should integrate cleanly into the same overall playback model as sample-backed voices.

That means the engine should preserve a coherent voice model even when the underlying data source differs.

### Architectural Importance

Streaming support is one of the reasons Carrot audio should continue treating:

* authored asset defaults
* runtime playback state
* decode/load work
* callback-safe mixing

as distinct concerns.

---

## 14. Platform Audio Backends

Carrot’s audio engine is intended to remain **backend-agnostic at the engine level**, while still using native platform audio APIs underneath.

### Why This Matters

The engine should not collapse its audio architecture into one platform-specific implementation.

Platform backends exist to:

* open audio devices
* configure callback/output behavior
* deliver output buffers
* interface with native platform audio systems

### Engine-Level Rule

Backends should not own the engine’s audio logic.

They should provide the platform-facing bridge for:

* device setup
* callback hookup
* output submission

while the engine retains ownership of:

* playback model
* mixing
* routing
* voice behavior
* DSP
* asset-driven defaults

That separation is important.

---

## 15. Audio and the Asset Pipeline

Carrot’s audio engine is tightly connected to the broader asset pipeline.

That relationship should remain clear.

### Asset Pipeline Role

The asset system is responsible for:

* identifying audio assets
* resolving source files
* interpreting authored metadata
* producing runtime-usable audio asset data

### Audio Engine Role

The audio engine is responsible for:

* playback
* mixing
* routing
* DSP
* runtime voice behavior

This separation matters.

The audio engine should not become a JSON parser, and the asset pipeline should not become the playback system.

For more detail, see `docs/systems/asset_pipeline.md`.

---

## 16. Future Tooling Direction

Carrot’s audio engine is intentionally being built in a way that can support stronger tooling later.

That does **not** mean the engine should rush into building a full custom audio editor immediately.

### Good Near-Term Direction

The current direction should continue favoring:

* strong authored metadata
* stable runtime architecture
* clear asset-driven defaults
* clean gameplay-facing APIs

### Future Possibilities

If and when they become worthwhile, future tooling may include:

* audio asset inspectors
* bus/mix debugging views
* runtime voice visualizers
* editor-side playback controls
* richer audio authoring workflows

The important thing is that the runtime architecture should make future tooling possible, not prevent it.

---

## 17. What This System Should Preserve

As Carrot evolves, the audio engine should continue preserving a few key ideas:

* audio is a first-class subsystem
* authored assets define defaults
* gameplay code expresses intent
* voices are runtime playback instances
* sample playback and streaming are distinct but unified concepts
* the engine mixes internally at 48 kHz
* buses and routing matter
* DSP is part of the architecture, not an afterthought
* command-driven control matters
* real-time safety constraints are non-negotiable
* platform backends should remain backend bridges, not engine logic owners

If those rules remain intact, the audio engine can evolve significantly without losing its identity.

---

## 18. Summary

Carrot’s audio engine is intended to provide a clean path from:

**authored sound assets**
→ **runtime playback voices**
→ **bus-based mixing and DSP**
→ **platform audio output**

That model supports:

* straightforward gameplay-facing usage
* strong runtime behavior
* real-time safety
* future tooling possibilities
* long-term engine growth

The goal is not just to “play sounds.”

The goal is to make Carrot audio:

* coherent
* expressive
* stable
* powerful
* pleasant to build with

## See Also

- `CARROT_MASTER_PLAN.md`
- `ARCHITECTURE_NOTES.md`
- `docs/systems/asset_pipeline.md`
- `docs/schemas/audio_asset_json_schema.md`
