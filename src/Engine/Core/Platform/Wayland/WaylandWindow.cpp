//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "WaylandWindow.h"

#include "xdg-shell-client-protocol.h"
#include "Events/Events.h"
#include "Input/PlatformKeyMapping.h"

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
            auto* win = static_cast<wayland_window_t *>(data);

            xdg_surface_ack_configure(xdg_surface, serial);

            if (win->configure_pending() && win->get_pending_width() > 0 && win->get_pending_height() > 0)
            {
                win->set_current_width(win->get_pending_width());
                win->set_current_height(win->get_pending_height());

                win->set_configure_pending(false);
            }
        }

        constexpr xdg_surface_listener xdg_surface_listener{ .configure = xdg_surface_configure };

        void xdg_toplevel_configure(void* data, xdg_toplevel*, const int32_t width, const int32_t height,
                                    [[maybe_unused]] wl_array* states)
        {
            auto* win = static_cast<wayland_window_t *>(data);

            if (width > 0 && height > 0)
            {
                win->set_pending_width(static_cast<uint32_t>(width));
                win->set_pending_height(static_cast<uint32_t>(height));
            }

            win->set_configure_pending(true);
        }

        void xdg_toplevel_close(void* data, xdg_toplevel*)
        {
            auto* win = static_cast<wayland_window_t *>(data);
            win->set_should_close(true);
        }

        constexpr xdg_toplevel_listener xdg_toplevel_listener{
            .configure = xdg_toplevel_configure,
            .close = xdg_toplevel_close,
            .configure_bounds = nullptr,
            .wm_capabilities = nullptr,
        };

        void pointer_motion(void* data, wl_pointer*, [[maybe_unused]] uint32_t time, const wl_fixed_t sx,
                            const wl_fixed_t sy)
        {
            wayland_window_t* win{ static_cast<wayland_window_t *>(data) };

            const chlm::float2 pos{
                static_cast<float>(wl_fixed_to_double(sx)), static_cast<float>(wl_fixed_to_double(sy))
            };
            const chlm::float2 delta{ pos - win->last_mouse_pos() };

            win->on_mouse_moved({ pos, delta });
            win->set_last_mouse_pos(pos);
        }

        void pointer_button(void* data, wl_pointer*, [[maybe_unused]] uint32_t serial, [[maybe_unused]] uint32_t time,
                            const uint32_t button, const uint32_t state_wl)
        {
            const wayland_window_t* win{ static_cast<wayland_window_t *>(data) };
            const input::mouse_button carrot_btn{ input::to_carrot_mouse_button(button) };

            const events::key_action action{
                state_wl == WL_POINTER_BUTTON_STATE_PRESSED ? events::key_action::press : events::key_action::release
            };

            win->on_mouse_button({ carrot_btn, action, win->last_mouse_pos() });
        }

        void pointer_axis(void* data, wl_pointer*, [[maybe_unused]] uint32_t time, const uint32_t axis,
                          const wl_fixed_t value)
        {
            wayland_window_t* win{ static_cast<wayland_window_t *>(data) };
            chlm::float2 delta{ 0.f, 0.f };

            if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
            {
                delta.y = static_cast<float>(-wl_fixed_to_double(value)); // usually invert Y for natural scroll
            }
            else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
            {
                delta.x = static_cast<float>(wl_fixed_to_double(value));
            }

            win->on_mouse_scrolled({ delta });
        }

        constexpr wl_pointer_listener pointer_listener = {
            .enter = [](void*, wl_pointer*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {},
            .leave = [](void*, wl_pointer*, uint32_t, wl_surface*) {},
            .motion = pointer_motion,
            .button = pointer_button,
            .axis = pointer_axis,
            .frame = nullptr,
            .axis_source = nullptr,
            .axis_stop = nullptr,
            .axis_discrete = nullptr,
            .axis_value120 = nullptr,
            .axis_relative_direction = nullptr
        };

        void keyboard_key(void* data, wl_keyboard*, [[maybe_unused]] uint32_t serial, [[maybe_unused]] uint32_t time,
                          const uint32_t key, const uint32_t state_wl)
        {
            auto* win = static_cast<wayland_window_t *>(data);

            auto carrot_key = input::to_carrot_key(key); // your mapping function

            events::key_action action;
            bool repeat = false;

            if (state_wl == WL_KEYBOARD_KEY_STATE_PRESSED)
            {
                action = events::key_action::press;
                if (win->key_down(static_cast<uint16_t>(carrot_key)))
                {
                    repeat = true; // simple repeat detection (improve later with timer)
                }
                win->set_key_down(static_cast<uint16_t>(carrot_key), true);
            }
            else
            {
                action = events::key_action::release;
                win->set_key_down(static_cast<uint16_t>(carrot_key), false);
            }

            win->on_key({ carrot_key, action, repeat, win->keyboard_mods() });
        }

        void keyboard_modifiers(void* data, wl_keyboard*, [[maybe_unused]] uint32_t serial, const uint32_t depressed,
                                [[maybe_unused]] uint32_t latched, [[maybe_unused]] uint32_t locked,
                                [[maybe_unused]] uint32_t group)
        {
            auto* win = static_cast<wayland_window_t *>(data);

            // Simple bitfield (adjust bits to your needs)
            uint8_t mods = 0;
            if (depressed & (1 << 0)) mods |= 1; // Shift (example mapping - use xkbcommon for real)
            // ... map other bits (Ctrl, Alt, Super)

            win->set_keyboard_mods(mods);
        }

        constexpr wl_keyboard_listener keyboard_listener = {
            .keymap = [](void*, wl_keyboard*, [[maybe_unused]] uint32_t format, [[maybe_unused]] int32_t fd,
                         [[maybe_unused]] uint32_t size) {
                /* handle keymap if using xkbcommon */
            },
            .enter = [](void*, wl_keyboard*, [[maybe_unused]] uint32_t serial, wl_surface*,
                        [[maybe_unused]] wl_array* keys) {},
            .leave = [](void*, wl_keyboard*, [[maybe_unused]] uint32_t serial, wl_surface*) {},
            .key = keyboard_key,
            .modifiers = keyboard_modifiers,
            .repeat_info = [](void*, wl_keyboard*, [[maybe_unused]] int32_t rate, [[maybe_unused]] int32_t delay) {
            },
        };

        constexpr wl_seat_listener seat_listener = {
            .capabilities = [](void* data, wl_seat* seat, uint32_t caps) {
                auto* win = static_cast<wayland_window_t *>(data);

                if (caps & WL_SEAT_CAPABILITY_KEYBOARD)
                {
                    win->set_keyboard(wl_seat_get_keyboard(seat));
                    wl_keyboard_add_listener(win->get_wl_keyboard(), &keyboard_listener, win);
                }

                if (caps & WL_SEAT_CAPABILITY_POINTER)
                {
                    win->set_pointer(wl_seat_get_pointer(seat));
                    wl_pointer_add_listener(win->get_wl_pointer(), &pointer_listener, win);
                }
            },
            .name = []([[maybe_unused]] void* data, [[maybe_unused]] wl_seat* seat, const char* name) {
                LOG_CORE_TRACE("Seat name: {}", name ? name : "unknown");
            }
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
            else if (std::strcmp(interface, wl_seat_interface.name) == 0)
            {
                win->set_seat(static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 4)));
                wl_seat_add_listener(win->get_wl_seat(), &seat_listener, win);
            }
        }

        void registry_global_remove(void*, wl_registry*, uint32_t) noexcept {}

        constexpr wl_registry_listener registry_listener{
            .global = registry_global,
            .global_remove = registry_global_remove,
        };
    } // anonymous

    wayland_window_t::wayland_window_t(const uint32_t width, const uint32_t height,
                                       const std::string_view title) noexcept
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
