//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "WaylandWindow.h"
#include "xdg-shell-client-protocol.h"

#include <wayland-client-protocol.h>
#include <string>

namespace carrot::core::platform {
    namespace {
        void xdg_wm_base_ping(void*, xdg_wm_base* shell, const uint32_t serial)
        {
            xdg_wm_base_pong(shell, serial);
        }

        constexpr xdg_wm_base_listener xdg_wm_base_listener{ .ping = xdg_wm_base_ping };

        void xdg_surface_configure(void* data, xdg_surface* xdg_surface, uint32_t serial)
        {
            auto* win = static_cast<wayland_window_t*>(data);

            xdg_surface_ack_configure(xdg_surface, serial);

            if (win->configure_pending() && win->get_pending_width() > 0 && win->get_pending_height() > 0)
            {
                win->set_current_width(win->get_pending_width());
                win->set_current_height(win->get_pending_height());

                win->set_configure_pending(false);
            }
        }

        constexpr xdg_surface_listener xdg_surface_listener{ .configure = xdg_surface_configure };

        void xdg_toplevel_configure(void* data, xdg_toplevel*, const int32_t width, const int32_t height, [[maybe_unused]] wl_array* states)
        {
            auto* win = static_cast<wayland_window_t*>(data);

            if (width > 0 && height > 0)
            {
                win->set_pending_width(static_cast<uint32_t>(width));
                win->set_pending_height(static_cast<uint32_t>(height));
            }

            win->set_configure_pending(true);
        }

        void xdg_toplevel_close(void* data, xdg_toplevel*)
        {
            auto* win = static_cast<wayland_window_t*>(data);
            win->set_should_close(true);
        }

        constexpr xdg_toplevel_listener xdg_toplevel_listener{
            .configure = xdg_toplevel_configure,
            .close = xdg_toplevel_close,
            .configure_bounds = nullptr,
            .wm_capabilities = nullptr,
        };

        void registry_global(void* data, wl_registry* registry, const uint32_t name,
                             const char* interface, uint32_t) noexcept
        {
            wayland_window_t* win{ static_cast<wayland_window_t *>(data) };

            if (std::strcmp(interface, wl_compositor_interface.name) == 0)
            {
                win->set_compositor(static_cast<wl_compositor *>(
                    wl_registry_bind(registry, name, &wl_compositor_interface, 4)));
            }
            else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0)
            {
                auto* base = static_cast<xdg_wm_base *>(
                    wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
                win->set_xdg_wm_base(base);
                xdg_wm_base_add_listener(base, &xdg_wm_base_listener, nullptr);
            }
        }

        void registry_global_remove(void*, wl_registry*, uint32_t) noexcept {}

        constexpr wl_registry_listener registry_listener{
            .global = registry_global,
            .global_remove = registry_global_remove,
        };
    } // anonymous

    wayland_window_t::wayland_window_t(const uint32_t width, const uint32_t height, const std::string_view title) noexcept
        : _current_width{ width }, _current_height{ height }
    {
        _display = wl_display_connect(nullptr);
        if (!_display) return;

        wl_registry* registry{ wl_display_get_registry(_display) };
        wl_registry_add_listener(registry, &registry_listener, this);
        wl_display_roundtrip(_display);

        if (!_compositor || !_xdg_wm_base) return;

        _surface = wl_compositor_create_surface(_compositor);
        _xdg_surface = xdg_wm_base_get_xdg_surface(_xdg_wm_base, _surface);
        xdg_surface_add_listener(_xdg_surface, &xdg_surface_listener, this);

        _xdg_toplevel = xdg_surface_get_toplevel(_xdg_surface);
        xdg_toplevel_add_listener(_xdg_toplevel, &xdg_toplevel_listener, this);
        xdg_toplevel_set_title(_xdg_toplevel, std::string(title).c_str());

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

    native_window_handle_t wayland_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ nullptr };
        handle.wayland_t.display = _display;
        handle.wayland_t.surface = _surface;

        return handle;
    }
} // namespace carrot::core::platform
