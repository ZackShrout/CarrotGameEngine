# Carrot Game Engine - Milestone 29

**Last Updated:** April 20, 2026
**Title:** Save, Load, and Durable Runtime State
**Status:** Planned
**Focus:** Establish Carrot's real save/load architecture with binary on-disk data, explicit serialization boundaries, stable slot structure, durable runtime-state ownership, and a practical vertical slice that proves the engine can persist real game progress safely.

---

## Milestone Goal

After Milestones 27 and 28, Carrot should be substantially closer to "real game-capable engine" territory.
The next missing piece is persistence.

Without a real save/load system, the engine can still feel like a demo platform even when movement, scenes, authored data, and runtime systems are growing well.

The next persistence question is:

* what state the engine owns versus what the game owns
* how that state is serialized without collapsing subsystem boundaries
* how save slots are structured and described
* how binary save data is versioned safely over engine growth
* how temp/auto/manual save behavior fits into the runtime honestly

Milestone 29 exists to give Carrot a real save/load foundation rather than a loose collection of file writes.

This milestone is successful if Carrot ends with:

* an engine-owned save/load service
* a clear serialization ownership line between engine systems and game systems
* stable save-slot structure and metadata
* binary payload storage with explicit versioning expectations
* practical support for temp save, autosave, and manual save slots
* a vertical slice proving that real progress can be saved, loaded, and resumed reliably

---

## Scope Summary

Milestone 29 is:

* a persistence architecture milestone
* a binary save-format milestone
* a save-slot structure milestone
* a runtime-state ownership milestone
* a migration/version-discipline milestone

Milestone 29 is not:

* a cloud-sync milestone
* a cross-platform account/profile service milestone
* a giant database subsystem
* a replay-recording milestone
* a broad editor tooling milestone

The key rule is:

**Milestone 29 should define what durable runtime state means in Carrot and make that state safely saveable/loadable without blurring engine/game ownership lines.**

---

## Why This Milestone Comes Next

Once Carrot can:

* render through its canonical 2D path
* move actors through a real gameplay-ready motion/collision architecture
* author richer world behavior and runtime state

the absence of save/load becomes a much larger structural limitation.

Persistence is the milestone where the engine has to answer:

* what progress actually means
* what state survives scene transitions, runtime rebuilds, and play sessions
* how game-facing systems integrate with engine-owned persistence

Carrot should answer those questions before piling on large amounts of reusable gameplay framework work, because later systems such as dialogue, cutscenes, battle state, quest progress, and inventories will all need durable boundaries to plug into.

---

## Core Architectural Rule

Carrot should not treat saving as "dump some globals to disk."

Instead:

* engine systems should serialize engine-owned durable state explicitly
* gameplay systems should serialize gameplay-owned durable state through clear registration/boundary seams
* transient runtime state should remain transient unless deliberately promoted to durable state
* save/load should restore a runtime from durable truth, not from ad hoc object snapshots with unclear ownership

If Milestone 29 writes files successfully but leaves ownership ambiguous, it has not gone far enough.

---

## Primary Deliverables

### 1. Engine-Owned Save Service

Carrot should expose a real save/load service rather than scattered file-writing helpers.

Required outcomes:

* save/load API owned by the engine runtime
* slot discovery/listing behavior
* save request / load request flow
* practical failure reporting

### 2. Binary Save Format and Slot Structure

Carrot should define stable save-slot structure and binary on-disk payload behavior.

Required outcomes:

* slot metadata separated from payload data
* binary payload container/layout
* version identifiers and compatibility policy
* clear file/layout ownership under the save root

### 3. Serialization Boundaries

Carrot should define who serializes what.

Required outcomes:

* engine-owned serialization seams
* gameplay-owned extension seam for durable state
* explicit rules for transient vs persistent runtime data
* less temptation to serialize arbitrary live object graphs without ownership discipline

### 4. Real Runtime-State Vertical Slice

Carrot should prove persistence through a real gameplay-facing slice, not just format scaffolding.

Required outcomes:

* scene/location restoration
* player-facing durable state restoration
* continuity data such as opened containers or named world/object flags restored
* at least one representative gameplay-owned state payload restored through the save system

### 5. Safety, Corruption Handling, and Version Discipline

Save/load should be resilient enough for real engine growth.

Required outcomes:

* invalid/corrupt save handling
* incompatible-version behavior
* sensible temp-write / finalize behavior where appropriate
* room for future migration logic

---

## Ticket Breakdown

### Ticket 29.1 - Save Service and Runtime Integration Boundary

**Priority:** P0
**Outcome:** Carrot has an engine-owned save/load service integrated with the runtime through clear request/response seams.

#### Why

If save/load begins as scattered subsystem logic, later growth will turn it into a hard-to-maintain ownership mess.

#### Scope

Define and implement the engine-owned save service:

* runtime-facing save/load API
* save root and slot discovery behavior
* load/apply flow owned by the runtime
* error/reporting path

#### Acceptance Criteria

* save/load is an engine service, not loose helper code
* the runtime knows how save/load requests are coordinated
* later systems have a stable place to integrate with persistence

### Ticket 29.2 - Slot Layout, Metadata, and Binary Payload Container

**Priority:** P0
**Outcome:** Save slots have a stable structure with metadata separated cleanly from binary durable-state payloads.

#### Why

A real game needs more than "one opaque file somewhere."
It needs discoverable slot truth and durable payload storage that can evolve.

#### Scope

Define the save-slot layout:

* slot identity and naming rules
* metadata file/record shape
* binary payload container or equivalent durable payload structure
* autosave/temp/manual slot distinctions where applicable

Likely metadata examples include:

* timestamp
* location/scene label
* playtime
* slot kind
* version/build compatibility information

#### Acceptance Criteria

* save slots have explicit structure
* metadata can be listed without loading full payload state
* binary payload storage is clearly owned and versioned

### Ticket 29.3 - Serialization Ownership and Registration Model

**Priority:** P0
**Outcome:** Carrot defines which engine systems serialize durable state and how game-side state plugs into that model.

#### Why

Save/load becomes fragile immediately if ownership lines are fuzzy.

The milestone must answer:

* what engine systems serialize directly
* what gameplay state is registered through a game-facing seam
* what state is intentionally transient

#### Scope

Define and implement the serialization ownership model for the validated slice:

* engine-owned durable state registration/participation
* gameplay-owned durable state participation
* durable vs transient state rules
* ordering/coordination for save and load application

#### Acceptance Criteria

* Carrot has a documented serialization boundary
* engine/game ownership is explicit
* save/load does not rely on ad hoc dumping of arbitrary runtime objects

### Ticket 29.4 - Scene, Continuity, and World-State Persistence

**Priority:** P0
**Outcome:** Core scene/world continuity state can be restored through the save system.

#### Why

Carrot already has real scene runtime and continuity concepts.
Persistence should build on those truths rather than inventing a parallel system.

#### Scope

Persist and restore the validated durable slice for:

* current scene/location identity
* spawn/entry restoration data where applicable
* world/object continuity flags
* durable changes such as opened containers or similar named world-state changes

#### Acceptance Criteria

* loading a save can restore the player back into meaningful world state
* continuity data survives across sessions
* persistence builds on engine-owned scene/runtime truths rather than bypassing them

### Ticket 29.5 - Gameplay-Owned Durable State Example Slice

**Priority:** P0
**Outcome:** At least one representative gameplay-owned state payload is persisted through the official save system.

#### Why

Milestone 29 should prove that game code can participate in persistence through clean boundaries.

#### Scope

Choose a representative gameplay-owned durable state slice for the milestone, such as:

* inventory-like state
* quest/state flags
* party/member state
* equivalent gameplay-owned runtime data

The exact example can stay narrow, but it should be real enough to validate the boundary.

#### Acceptance Criteria

* at least one gameplay-owned durable-state slice is saved and restored
* it uses the official serialization boundary
* the result is not a one-off hack around the engine service

### Ticket 29.6 - Temp Save, Autosave, and Manual Save Flow

**Priority:** P1
**Outcome:** Carrot distinguishes the practical save behaviors a real game needs.

#### Why

Persistence design should not assume every save is the same kind of operation.

#### Scope

Add the milestone's practical save-flow distinctions:

* temp/continue-style save
* autosave slot behavior
* manual save slot behavior

This does not need to become a full UI milestone.
It does need to become a real runtime concept.

#### Acceptance Criteria

* the runtime can distinguish temp/autosave/manual behavior
* slot metadata and load/list behavior reflect those distinctions clearly
* the save system does not assume only one slot type exists

### Ticket 29.7 - Save Safety, Corruption Handling, and Compatibility Behavior

**Priority:** P1
**Outcome:** Carrot handles failure and incompatibility honestly instead of assuming every save file is valid forever.

#### Why

A save system that works only when nothing goes wrong is not enough for real engine use.

#### Scope

Add the milestone's safety behavior:

* invalid/corrupt save detection
* unsupported version behavior
* safe write/finalize flow where practical
* clear load failure reporting

#### Acceptance Criteria

* corrupt or incompatible saves fail in a controlled and understandable way
* the engine can distinguish compatibility problems from ordinary file-missing problems
* save writing is structured more safely than naive direct overwrite

### Ticket 29.8 - Persistence Diagnostics and Verification Coverage

**Priority:** P1
**Outcome:** Save/load behavior is testable and diagnosable rather than opaque.

#### Why

Persistence bugs are some of the most painful bugs to reason about if the system lacks visibility and tests.

#### Scope

Add diagnostics and regression coverage for:

* slot discovery and metadata
* binary payload round-trip behavior
* engine-owned state restore
* gameplay-owned state restore
* version/corruption failure behavior

#### Acceptance Criteria

* save/load coverage exists for the validated milestone slice
* persistence failures can be investigated with meaningful diagnostics
* the milestone's ownership rules are protected against accidental regression

---

## Vertical Slice Expectations

Milestone 29 should prove itself through a real runtime slice, not just a file-format demo.

A successful milestone validation slice should include at least:

* load into the correct scene/location
* restored player/world continuity state
* restored named durable object/world flags
* restored gameplay-owned durable state example
* slot listing that shows metadata without fully loading the save payload
* a temp/autosave/manual distinction present in the runtime truth

---

## Non-Goals

Milestone 29 should explicitly not grow into:

* network/cloud saves
* account systems
* giant save-editing tools
* full modding/import/export frameworks
* a broad dialogue/cutscene/battle milestone
* every future gameplay module that will eventually want persistence

Those are all legitimate future consumers of save/load.
They are not the purpose of this milestone.

---

## Looking Ahead - Early Milestone 30 Direction

Milestones 27 through 29 are already large engine-shaping milestones.
Milestone 30 therefore should not be overcommitted yet.

However, Carrot should not lose sight of the little it already knows about the next likely growth area after persistence lands.

Current early Milestone 30 direction:

* reusable gameplay framework modules
* optional puzzle-piece systems that game code can adopt or ignore
* likely first candidates:
  * dialogue system foundation
  * cutscene/sequencing foundation
  * first battle-system framework direction

Important current boundary:

* Milestone 30 should likely build on top of the save/load architecture from Milestone 29 rather than inventing parallel persistence for gameplay modules
* Milestone 30 should remain modular and game-optional rather than forcing one giant gameplay framework on every Carrot project
* the exact ordering between dialogue, cutscene, and battle framework work is intentionally not locked yet

This note exists so that, when Milestone 29 is approaching completion, Carrot has a visible reminder of the next likely engine-facing gameplay framework direction without pretending the milestone is already fully designed.

---

## Validation Expectations

Milestone 29 should not be considered complete until it validates:

* save slot listing and metadata behavior
* binary payload round-trip for the validated slice
* scene/world continuity restore
* gameplay-owned durable-state restore
* temp/autosave/manual save distinctions
* corruption and incompatibility behavior

---

## Success Criteria

Milestone 29 is succeeding when:

* Carrot has a real engine-owned save/load service
* save slots and metadata have a clear durable structure
* binary payload ownership and versioning are explicit
* engine-owned and gameplay-owned serialization boundaries are clear
* the runtime can restore meaningful real progress, not just trivial demo state
* the milestone leaves later gameplay modules with a clean persistence foundation to build on

That would make Milestone 29 the point where Carrot becomes capable of durable game progress instead of only moment-to-moment runtime play.
