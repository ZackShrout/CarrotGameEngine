#pragma once

#include <cstdint>
#include <Engine/Core/Platform/Wayland/WaylandWindow.h>

namespace carrot::window {

void create_primary_window(uint32_t width, uint32_t height, const char* title) noexcept;
void destroy_primary_window() noexcept;
void poll_events() noexcept;

[[nodiscard]] carrot::platform::wayland_window_t& get_primary_window() noexcept;
[[nodiscard]] bool should_close() noexcept;

} // namespace carrot::window