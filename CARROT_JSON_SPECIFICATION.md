# Carrot JSON Specification (CJS)

## Purpose

The Carrot JSON Specification defines a **restricted, intentional subset of JSON** supported by the Carrot engine. Its goal is not general-purpose JSON compatibility, but **clarity, robustness, and long-term maintainability** for engine-owned configuration and asset import pipelines.

JSON under this specification is **never used at runtime**. All JSON parsing occurs at **import time** or **engine startup**, and data is converted into internal, engine-defined binary or native formats immediately after parsing.

This document is a design contract, not just an implementation note. It exists to prevent scope creep and accidental complexity over the lifetime of the engine.

---

## Design Pillars

1. **Import-Time Only**  
   JSON is used exclusively for:
   - Engine configuration
   - Asset import pipelines
   - External tool interoperability (e.g. Aseprite, Tiled)

   JSON data must never persist beyond initialization or asset conversion.

2. **Engine-Controlled Scope**  
   The Carrot engine explicitly defines which JSON features are supported. Unsupported constructs are treated as errors or warnings, not silently ignored.

3. **Fail Loudly, Recover Gracefully**  
   Invalid or unsupported JSON must:
   - Produce clear diagnostics
   - Provide line/column context where possible
   - Allow sensible fallback behavior when appropriate

4. **Minimal Surface Area**  
   Every supported feature must justify its existence. Convenience is acceptable; generality is not.

5. **Replaceable by Design**  
   The JSON system is accessed only through a Carrot-defined API. The underlying implementation may be replaced without affecting engine users.

---

## Supported JSON Grammar

The Carrot JSON subset is based on RFC 8259 with deliberate omissions.

### Supported Value Types

- **Object**: `{ string : value, ... }`
- **Array**: `[ value, value, ... ]`
- **String**: UTF-8 encoded
- **Number**: 64-bit signed integers and 64-bit floating point
- **Boolean**: `true`, `false`
- **Null**: `null`

### Unsupported Features

The following are intentionally **not supported**:

- Comments (//, /* */, or variants)
- Trailing commas
- NaN / Infinity numeric values
- Arbitrary precision numbers
- Duplicate object keys (behavior is undefined and may error)
- JSON Schema, `$ref`, or validation keywords

---

## Parsing Model

### Parsing Strategy

- Full document parse (no streaming)
- In-memory representation valid only for import duration
- No incremental or partial parsing

### AST Lifetime

- JSON AST nodes are short-lived
- Designed for immediate traversal and conversion
- Explicit destruction or arena reset after use

---

## Error Handling

### Syntax Errors

- Invalid syntax must abort parsing
- Error messages should include:
  - File path
  - Line number
  - Column number

### Semantic Errors

Examples:
- Unknown enum strings
- Invalid value types
- Missing required keys

Behavior:
- Engine-owned schemas: error or fallback (defined per schema)
- External formats: error or import failure

---

## Engine-Owned Schemas

Schemas owned by the engine (e.g. config files) may define:

- Required keys
- Optional keys with defaults
- Explicit string-to-enum mappings

Example:
```json
{
  "graphics": {
    "api": "default",
    "debug_layers": "default"
  }
}
```

The string value `"default"` is a first-class concept interpreted by engine logic, not the JSON system itself.

---

## Foreign JSON Formats

Some tools emit JSON schemas outside Carrot's control.

Examples:
- Aseprite
- Tiled
- LDtk

Policy:
- Parsed using the same JSON infrastructure
- Converted immediately to internal formats
- JSON representation discarded permanently

Highly complex formats (e.g. glTF) are treated as **protocols**, not native Carrot JSON, and may use dedicated importers.

---

## Performance Expectations

- Parsing performance is not a primary concern
- Memory usage is acceptable to be transiently higher during import
- No optimizations should compromise clarity or debuggability

---

## Implementation Guidance

### Placement in Engine

- Implemented as a **compiled C++ module**
- Not header-only
- Integrated directly into the engine

Rationale:
- Error reporting requires state
- Tokenization benefits from private implementation
- AST management benefits from controlled allocation

### API Boundaries

All interaction with JSON must go through a Carrot-defined interface.

Example concepts:
- `json_document`
- `json_value`
- `json_object_view`
- `json_array_view`

No engine subsystem may depend on third-party JSON APIs directly.

---

## Versioning

This specification is versioned independently of the engine.

- Backward compatibility is not guaranteed
- Asset import pipelines must specify the expected CJS version if stability is required

---

## Non-Goals

Carrot JSON explicitly does **not** aim to:

- Be a general-purpose JSON library
- Support runtime data-driven gameplay systems
- Replace specialized importers for complex formats

---

## Summary

The Carrot JSON Specification exists to:

- Enable clean, readable config and import pipelines
- Prevent accidental architectural creep
- Keep the engine honest about what it actually needs

If a feature feels "nice to have" but not essential, it does not belong here.

