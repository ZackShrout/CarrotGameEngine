#pragma once

#include <cstdint>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct wl_egl_window;

namespace carrot::platform {

struct wayland_window
{
public:
    explicit wayland_window(uint32_t width, uint32_t height, const char* title) noexcept;
    ~wayland_window() noexcept;

    [[nodiscard]] bool should_close() const noexcept;
    void poll_events() noexcept;

    // Vulkan WSI
    [[nodiscard]] wl_display* get_wl_display() const noexcept { return _display; }
    [[nodiscard]] wl_surface* get_wl_surface() const noexcept { return _surface; }

    // Needed by the static listener callback
    void handle_registry_global(wl_registry* registry,
                                uint32_t name,
                                const char* interface,
                                uint32_t version) noexcept;

private:
    wl_display*    _display{ nullptr };
    wl_registry*   _registry{ nullptr };
    wl_compositor* _compositor{ nullptr };
    wl_surface*    _surface{ nullptr };
    wl_egl_window* _native_window{ nullptr };

    bool _should_close{ false };
};

} // namespace carrot::platform