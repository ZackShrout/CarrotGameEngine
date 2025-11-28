#include "Engine/Core/Platform/Wayland/WaylandWindow.h"

#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <cstring>
#include <cstdio>

namespace carrot::platform {
namespace {

static void registry_global(void* data,
                            wl_registry* registry,
                            uint32_t name,
                            const char* interface,
                            uint32_t version) noexcept
{
    auto* window = static_cast<wayland_window*>(data);
    window->handle_registry_global(registry, name, interface, version);
}

static void registry_global_remove(void*, wl_registry*, uint32_t) noexcept {}

static constexpr wl_registry_listener k_registry_listener{
    .global        = registry_global,
    .global_remove = registry_global_remove,
};

} // anonymous namespace

void wayland_window::handle_registry_global(wl_registry* registry,
                                            uint32_t name,
                                            const char* interface,
                                            uint32_t) noexcept
{
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        _compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
    }
}

wayland_window::wayland_window(uint32_t width, uint32_t height, const char*) noexcept
{
    _display = wl_display_connect(nullptr);
    if (!_display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return;
    }

    _registry = wl_display_get_registry(_display);
    wl_registry_add_listener(_registry, &k_registry_listener, this);
    wl_display_roundtrip(_display);

    if (!_compositor) {
        fprintf(stderr, "Failed to get wl_compositor\n");
        return;
    }

    _surface = wl_compositor_create_surface(_compositor);
    if (!_surface) {
        fprintf(stderr, "Failed to create wl_surface\n");
        return;
    }

    _native_window = wl_egl_window_create(_surface,
                                          static_cast<int32_t>(width),
                                          static_cast<int32_t>(height));
}

wayland_window::~wayland_window() noexcept
{
    if (_native_window) wl_egl_window_destroy(_native_window);
    if (_surface) wl_surface_destroy(_surface);
    if (_registry) wl_registry_destroy(_registry);
    if (_display) wl_display_disconnect(_display);
}

bool wayland_window::should_close() const noexcept { return _should_close; }

void wayland_window::poll_events() noexcept
{
    wl_display_dispatch_pending(_display);
}

} // namespace carrot::platform