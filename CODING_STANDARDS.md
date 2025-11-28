# CARROT GAME ENGINE – CODING STANDARDS (v1.0 – FINAL)
## Locked November 27, 2025 – BunnySoft

### File & Folder Naming
- Headers:        `.h`
- Source:         `.cpp`
- Folders under `src/`: UpperCamelCase (Engine/, Renderer/, ECS/, etc.)
- Source files:   UpperCamelCase (Window.h / Window.cpp, Entity.h / Entity.cpp)

### Naming Conventions
- All user-defined types (struct, class, enum class, typedef, using): `snake_case_t`
- Functions, variables, parameters, namespaces:                     `snake_case`
- Private/protected member variables:                              `_snake_case`
- Member functions:                                                `snake_case`
- Compile-time constants / static globals:                          `k_max_entities`, `g_frames_in_flight`
- Macros (rare, screaming case only when unavoidable):            `CARROT_ENABLE_TRACY`

### Core Language Style
- Uniform/bracket initialization everywhere:   `int32_t frame_count{0}; float delta_time{0.f};`
- Float literals always suffixed:               `0.f`, `1.f`, `3.14159f`
- `auto` only when:
  - iterating containers: `for (const auto& entity : entities)`
  - the type name is unreasonably long and obvious from context
  - never used to hide pointer/reference levels
- Single-statement `if` / `for` / `while` → no braces
  ```cpp
  if (is_valid) return true;
    for (uint32_t i{0}; i < count; ++i) do_thing(i);
  ```
- Multi-statement blocks always braced and newline-aligned (Allman style)
- `constexpr`, `const`, `noexcept`, `[[nodiscard]]`, `[[nodiscard("reason")]]` applied aggressively wherever legal
- `goto` forbidden except the classic “goto cleanup” pattern in functions with multiple early returns and manual resource cleanup (we will have a few of these early on before RAII is complete)

### Type Discipline
- Prefer struct over class
- Use class only when private sections + inheritance are actually required
- Free functions in namespaces strongly preferred over member functions when behaviour is not tied to internal state

### Struct / Class Layout Order (exact)
```cpp
struct foo
{
public:     // public interface first
    // public methods
    // public data (rare, only if truly POD)

protected:  // (only if used)
    // protected methods
    // protected data

private:    // everything else
    // private methods
    // private data
};
```

### Namespaces
- All engine code lives under namespace carrot { ... }
- Sub-namespaces preferred over deep type names (carrot::rhi, carrot::ecs, etc.)
- Anonymous namespaces used heavily in .cpp files for file-local symbols
- Never using namespace in headers
- using declarations allowed in .cpp files after the anonymous namespace when it is safe and improves readability

### Other Mandatory Rules
- Pointers and references stick to the type: `int32_t* ptr`, `float& value` (East-const style forbidden)
- Include guards: `#pragma once`
- Forward declarations preferred over unnecessary includes in headers
- Every public-facing function that cannot fail gets `[[nodiscard]]`
- Every function that does not allocate or throw gets `noexcept` when possible
- Zero raw new/delete after bootstrap phase – we will use our own allocators
- All engine subsystems are modules with explicit `init()` / `shutdown()` (enables hot-reload later)

### Example of Perfect Carrot Style
```cpp
#pragma once

#include <cstdint>

namespace carrot::ecs {

using entity_id = uint32_t;

struct transform_component
{
public:
    void set_position(float x, float y, float z) noexcept;
    [[nodiscard]] bool is_static() const noexcept { return _is_static; }

private:
    float    _position[3]{ 0.f, 0.f, 0.f };
    float    _rotation[4]{ 0.f, 0.f, 0.f, 1.f };
    float    _scale[3]{ 1.f, 1.f, 1.f };
    bool     _is_static{ false };
};

} // namespace carrot::ecs
```