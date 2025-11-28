#include "Engine/Core/Platform/Wayland/WaylandWindow.h"
#include "Engine/Core/Platform/Wayland/xdg-shell-client-protocol.h"
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <cstring>
#include <cstdio>

namespace carrot::platform {

namespace {

void xdg_wm_base_ping(void*, xdg_wm_base* shell, uint32_t serial)
{ xdg_wm_base_pong(shell, serial); }

const xdg_wm_base_listener xdg_wm_base_listener{ .ping = xdg_wm_base_ping };

void xdg_surface_configure(void*, xdg_surface*, uint32_t) {}
const xdg_surface_listener xdg_surface_listener{ .configure = xdg_surface_configure };

void xdg_toplevel_configure(void*, xdg_toplevel*, int32_t, int32_t, wl_array*) {}
void xdg_toplevel_close(void*, xdg_toplevel*) {}

const xdg_toplevel_listener xdg_toplevel_listener{
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

void registry_global(void* data, wl_registry* registry, uint32_t name,
                     const char* interface, uint32_t) noexcept
{
    auto* win = static_cast<wayland_window_t*>(data);

    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        win->set_compositor(static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4)));
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        auto* base = static_cast<xdg_wm_base*>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        win->set_xdg_wm_base(base);
        xdg_wm_base_add_listener(base, &xdg_wm_base_listener, nullptr);
    }
}

void registry_global_remove(void*, wl_registry*, uint32_t) noexcept {}

const wl_registry_listener registry_listener{
    .global = registry_global,
    .global_remove = registry_global_remove,
};

} // anonymous

wayland_window_t::wayland_window_t(uint32_t width, uint32_t height, const char* title) noexcept
{
    _display = wl_display_connect(nullptr);
    if (!_display) return;

    auto* registry = wl_display_get_registry(_display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(_display);

    if (!_compositor || !_xdg_wm_base) return;

    _surface = wl_compositor_create_surface(_compositor);
    _xdg_surface = xdg_wm_base_get_xdg_surface(_xdg_wm_base, _surface);
    xdg_surface_add_listener(_xdg_surface, &xdg_surface_listener, nullptr);

    _xdg_toplevel = xdg_surface_get_toplevel(_xdg_surface);
    xdg_toplevel_add_listener(_xdg_toplevel, &xdg_toplevel_listener, nullptr);
    xdg_toplevel_set_title(_xdg_toplevel, title);

    wl_surface_commit(_surface);
    wl_display_roundtrip(_display);
}

wayland_window_t::~wayland_window_t() noexcept
{
    if (_xdg_toplevel) xdg_toplevel_destroy(_xdg_toplevel);
    if (_xdg_surface) xdg_surface_destroy(_xdg_surface);
    if (_xdg_wm_base) xdg_wm_base_destroy(_xdg_wm_base);
    if (_surface) wl_surface_destroy(_surface);
    if (_display) wl_display_disconnect(_display);
}

void wayland_window_t::poll_events() noexcept
{
    wl_display_dispatch_pending(_display);
}

} // namespace carrot::platform
