#pragma once
#include <cstdint>
#include <wayland-client.h>

struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;

namespace carrot::platform {

struct wayland_window_t
{
public:
    explicit wayland_window_t(uint32_t width, uint32_t height, const char* title) noexcept;
    ~wayland_window_t() noexcept;

    void poll_events() noexcept;
    [[nodiscard]] bool should_close() const noexcept { return _should_close; }

    [[nodiscard]] wl_display* get_wl_display() const noexcept { return _display; }
    [[nodiscard]] wl_surface* get_wl_surface() const noexcept { return _surface; }

    // These two are only for the registry callback
    void set_compositor(wl_compositor* c)   noexcept { _compositor = c; }
    void set_xdg_wm_base(xdg_wm_base* base) noexcept { _xdg_wm_base = base; }

private:
    wl_display*      _display{ nullptr };
    wl_compositor*   _compositor{ nullptr };
    wl_surface*      _surface{ nullptr };
    xdg_wm_base*     _xdg_wm_base{ nullptr };
    xdg_surface*     _xdg_surface{ nullptr };
    xdg_toplevel*    _xdg_toplevel{ nullptr };

    bool _should_close{ false };
};

} // namespace carrot::platform
