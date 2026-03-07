//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "WaylandWindow.h"

#include "xdg-shell-client-protocol.h"
#include "Events/Events.h"
#include "Input/PlatformKeyMapping.h"

#include <wayland-client-protocol.h>
#include <sys/mman.h>
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

            if (win->get_configure_pending() && win->get_pending_width() > 0 && win->get_pending_height() > 0)
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
            const chlm::float2 delta{ pos - win->get_last_mouse_pos() };

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

            win->on_mouse_button({ carrot_btn, action, win->get_last_mouse_pos() });
        }

        void pointer_axis(void* data, wl_pointer*, [[maybe_unused]] uint32_t time, const uint32_t axis,
                          const wl_fixed_t value)
        {
            const wayland_window_t* win{ static_cast<wayland_window_t *>(data) };
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

        bool is_repeatable_key(const input::key_code key)
        {
            switch (key)
            {
                case input::key_code::left_shift:
                case input::key_code::right_shift:
                case input::key_code::left_control:
                case input::key_code::right_control:
                case input::key_code::left_alt:
                case input::key_code::right_alt:
                case input::key_code::left_super:
                case input::key_code::right_super:
                case input::key_code::escape:
                case input::key_code::f1:
                case input::key_code::f2:
                case input::key_code::f3:
                case input::key_code::f4:
                case input::key_code::f5:
                case input::key_code::f6:
                case input::key_code::f7:
                case input::key_code::f8:
                case input::key_code::f9:
                case input::key_code::f10:
                case input::key_code::f11:
                case input::key_code::f12:
                    return false;
                default:
                    return true;
            }
        }

        void keyboard_key(void* data, wl_keyboard*, [[maybe_unused]] uint32_t serial, [[maybe_unused]] uint32_t time,
                          const uint32_t key, const uint32_t state_wl)
        {
            wayland_window_t* win{ static_cast<wayland_window_t *>(data) };
            const input::key_code carrot_key{ input::to_carrot_key(key) };
            constexpr bool repeat{ false };

            const events::key_action action{
                state_wl == WL_KEYBOARD_KEY_STATE_PRESSED ? events::key_action::press : events::key_action::release
            };

            const events::key_event_t evt{ carrot_key, action, repeat, win->get_keyboard_mods() };
            win->on_key(evt);

            // ────────────── Repeat management ──────────────
            if (action == events::key_action::press)
            {
                if (win->get_repeat_enabled() && is_repeatable_key(carrot_key))
                {
                    win->set_repeat_state_key(carrot_key);
                    win->set_repeat_state_active(true);
                    win->set_repeat_state_last_time(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count()); // start delay countdown

                    win->set_repeat_state_first_sent(false);
                }
            }
            else // Release
            {
                if (win->get_repeat_state()._key == carrot_key)
                    win->set_repeat_state_active(false);
            }
        }

        void keyboard_modifiers(void* data, wl_keyboard*, [[maybe_unused]] uint32_t serial, const uint32_t depressed,
                                const uint32_t latched, const uint32_t locked, const uint32_t group)
        {
            wayland_window_t* win{ static_cast<wayland_window_t *>(data) };

            // Tell xkbcommon about the new modifier state
            xkb_state_update_mask(win->get_xkb_state(),
                                  depressed, latched, locked,
                                  0, 0, group); // group = layout index

            // Now query effective modifier bits
            uint8_t mods = 0;

            if (xkb_state_mod_name_is_active(win->get_xkb_state(), XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE))
                mods |= static_cast<uint8_t>(input::modifier::shift);
            if (xkb_state_mod_name_is_active(win->get_xkb_state(), XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE))
                mods |= static_cast<uint8_t>(input::modifier::control);
            if (xkb_state_mod_name_is_active(win->get_xkb_state(), XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE))
                mods |= static_cast<uint8_t>(input::modifier::alt);
            if (xkb_state_mod_name_is_active(win->get_xkb_state(), XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE))
                mods |= static_cast<uint8_t>(input::modifier::super); // (Win/Cmd)

            win->set_keyboard_mods(mods);
        }

        void keyboard_repeat_info(void* data, wl_keyboard*, const int32_t rate, const int32_t delay)
        {
            wayland_window_t* win{ static_cast<wayland_window_t *>(data) };

            if (rate <= 0)
            {
                win->set_repeat_enabled(false);
                win->set_repeat_state_active(false);
                return;
            }

            win->set_repeat_enabled(true);
            win->set_repeat_state_delay(static_cast<uint32_t>(delay));
            win->set_repeat_state_rate(1000 / static_cast<uint32_t>(rate)); // e.g. 33 ms for 30 Hz
        }

        void keyboard_keymap(void* data, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size)
        {
            wayland_window_t* win{ static_cast<wayland_window_t *>(data) };

            if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
            {
                LOG_CORE_ERROR("Unsupported keymap format: {}", format);
                close(fd);
                return;
            }

            char* map_str = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
            if (map_str == MAP_FAILED)
            {
                LOG_CORE_ERROR("Failed to mmap keymap");
                close(fd);
                return;
            }

            win->set_xkb_keymap(xkb_keymap_new_from_string(
                win->get_xkb_context(),
                map_str,
                XKB_KEYMAP_FORMAT_TEXT_V1,
                XKB_KEYMAP_COMPILE_NO_FLAGS
            ));

            munmap(map_str, size);
            close(fd);

            if (!win->get_xkb_keymap())
            {
                LOG_CORE_ERROR("Failed to compile xkb keymap");
                return;
            }

            win->set_xkb_state(xkb_state_new(win->get_xkb_keymap()));
            if (!win->get_xkb_state())
            {
                LOG_CORE_ERROR("Failed to create xkb state");
                xkb_keymap_unref(win->get_xkb_keymap());
                win->set_xkb_keymap(nullptr);
            }
        }

        constexpr wl_keyboard_listener keyboard_listener = {
            .keymap = keyboard_keymap,
            .enter = [](void*, wl_keyboard*, [[maybe_unused]] uint32_t serial, wl_surface*,
                        [[maybe_unused]] wl_array* keys) {},
            .leave = [](void*, wl_keyboard*, [[maybe_unused]] uint32_t serial, wl_surface*) {},
            .key = keyboard_key,
            .modifiers = keyboard_modifiers,
            .repeat_info = keyboard_repeat_info
        };

        constexpr wl_seat_listener seat_listener = {
            .capabilities = [](void* data, wl_seat* seat, const uint32_t caps) {
                auto* win = static_cast<wayland_window_t *>(data);

                if (caps & WL_SEAT_CAPABILITY_KEYBOARD)
                {
                    win->set_keyboard(wl_seat_get_keyboard(seat));

                    // ────────────── xkbcommon setup ──────────────
                    win->set_xkb_context(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
                    if (!win->get_xkb_context())
                    {
                        LOG_CORE_ERROR("Failed to create xkb_context");
                        return;
                    }

                    wl_keyboard_add_listener(win->get_wl_keyboard(), &keyboard_listener, win);
                }

                if (caps & WL_SEAT_CAPABILITY_POINTER)
                {
                    win->set_pointer(wl_seat_get_pointer(seat));
                    wl_pointer_add_listener(win->get_wl_pointer(), &pointer_listener, win);
                }
            },
            .name = []([[maybe_unused]] void* data, [[maybe_unused]] wl_seat* seat,
                       [[maybe_unused]] const char* name) {}
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
    {
        _width = width;
        _height = height;
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

        wl_registry_destroy(registry);
    }

    wayland_window_t::~wayland_window_t() noexcept
    {
        if (_keyboard) wl_keyboard_destroy(_keyboard);
        if (_pointer) wl_pointer_destroy(_pointer);
        if (_xkb_state) xkb_state_unref(_xkb_state);
        if (_xkb_keymap) xkb_keymap_unref(_xkb_keymap);
        if (_xkb_context) xkb_context_unref(_xkb_context);
        if (_xdg_toplevel) xdg_toplevel_destroy(_xdg_toplevel);
        if (_xdg_surface) xdg_surface_destroy(_xdg_surface);
        if (_xdg_wm_base) xdg_wm_base_destroy(_xdg_wm_base);
        if (_seat) wl_seat_destroy(_seat);
        if (_compositor) wl_compositor_destroy(_compositor);
        if (_surface) wl_surface_destroy(_surface);
        if (_display) wl_display_disconnect(_display);
    }

    void wayland_window_t::poll_events() noexcept
    {
        if (!_display) return;

        // 1. Dispatch all pending Wayland events (keys, buttons, motion, scroll, modifiers, etc.)
        wl_display_dispatch_pending(_display);
        wl_display_flush(_display);

        // 2. Generate synthetic repeats (only if we have an active repeating key)
        if (_repeat_state._active && _repeat_state._key != input::key_code::unknown)
        {
            const uint64_t now_ms{
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count())
            };

            const uint64_t time_since_last = now_ms - _repeat_state._last_time_ms;
            bool should_send_repeat = false;

            if (!_repeat_state._first_repeat_sent)
            {
                // Waiting for initial delay
                if (time_since_last >= _repeat_state._delay_ms)
                {
                    should_send_repeat = true;
                    _repeat_state._first_repeat_sent = true;
                }
            }
            else
            {
                // Waiting for next interval
                if (time_since_last >= _repeat_state._rate_ms)
                    should_send_repeat = true;
            }

            if (should_send_repeat)
            {
                const events::key_event_t evt{
                    _repeat_state._key,
                    events::key_action::repeat,
                    true,
                    _keyboard_mods
                };

                on_key(evt);
                _repeat_state._last_time_ms = now_ms;
            }
        }
    }

    void wayland_window_t::set_title(std::string_view basic_string_view) noexcept
    {
        window_t::set_title(basic_string_view);
    }

    void wayland_window_t::minimize() noexcept
    {
        window_t::minimize();
    }

    void wayland_window_t::maximize() noexcept
    {
        window_t::maximize();
    }

    void wayland_window_t::restore() noexcept
    {
        window_t::restore();
    }

    bool wayland_window_t::is_maximized() const noexcept
    {
        return window_t::is_maximized();
    }

    bool wayland_window_t::is_minimized() const noexcept
    {
        return window_t::is_minimized();
    }

    native_window_handle_t wayland_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ nullptr };
        handle.wayland_t.display = _display;
        handle.wayland_t.surface = _surface;

        return handle;
    }

    bool wayland_window_t::is_fullscreen() const noexcept
    {
        return window_t::is_fullscreen();
    }

    void wayland_window_t::set_fullscreen(const bool fullscreen) noexcept
    {
        window_t::set_fullscreen(fullscreen);
    }
} // namespace carrot::core::platform
