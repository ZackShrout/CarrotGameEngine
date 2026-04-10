//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "WaylandWindow.h"

#include "Events/Events.h"
#include "Input/PlatformKeyMapping.h"
#include "Protocols/xdg-decoration-unstable-v1-client-protocol.h"
#include "Protocols/xdg-shell-client-protocol.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <sys/mman.h>
#include <wayland-client-protocol.h>

#ifdef CARROT_HAS_LIBDECOR
#include <libdecor.h>
#endif

namespace carrot::core::platform {
    namespace {
        struct shared_wayland_state_t
        {
            wl_display* display{ nullptr };
            wl_registry* registry{ nullptr };
            wl_compositor* compositor{ nullptr };
            xdg_wm_base* xdg_wm_base{ nullptr };
            uint32_t seat_name{ 0 };
            uint32_t seat_version{ 0 };
            zxdg_decoration_manager_v1* decoration_manager{ nullptr };
#ifdef CARROT_HAS_LIBDECOR
            libdecor* libdecor_context{ nullptr };
#endif
            uint32_t ref_count{ 0 };
            uint64_t poll_generation{ 0 };
            uint64_t last_pumped_generation{ 0 };
            std::vector<wayland_window_t*> windows;
        };

        shared_wayland_state_t g_shared_wayland;

        void xdg_wm_base_ping(void*, xdg_wm_base* shell, const uint32_t serial)
        {
            xdg_wm_base_pong(shell, serial);
        }

        constexpr xdg_wm_base_listener xdg_wm_base_listener{ .ping = xdg_wm_base_ping };

        [[nodiscard]] bool acquire_shared_wayland_state()
        {
            if (g_shared_wayland.ref_count++ > 0)
                return g_shared_wayland.display != nullptr && g_shared_wayland.registry != nullptr;

            g_shared_wayland = shared_wayland_state_t{ };
            g_shared_wayland.ref_count = 1;
            g_shared_wayland.display = wl_display_connect(nullptr);
            if (!g_shared_wayland.display)
                return false;

            g_shared_wayland.registry = wl_display_get_registry(g_shared_wayland.display);
            if (!g_shared_wayland.registry)
                return false;

            return true;
        }

        void release_shared_wayland_state() noexcept
        {
            if (g_shared_wayland.ref_count == 0)
                return;

            if (--g_shared_wayland.ref_count > 0)
                return;

#ifdef CARROT_HAS_LIBDECOR
            if (g_shared_wayland.libdecor_context)
            {
                libdecor_unref(g_shared_wayland.libdecor_context);
                g_shared_wayland.libdecor_context = nullptr;
            }
#endif

            if (g_shared_wayland.decoration_manager)
            {
                zxdg_decoration_manager_v1_destroy(g_shared_wayland.decoration_manager);
                g_shared_wayland.decoration_manager = nullptr;
            }

            if (g_shared_wayland.xdg_wm_base)
            {
                xdg_wm_base_destroy(g_shared_wayland.xdg_wm_base);
                g_shared_wayland.xdg_wm_base = nullptr;
            }

            if (g_shared_wayland.compositor)
            {
                wl_compositor_destroy(g_shared_wayland.compositor);
                g_shared_wayland.compositor = nullptr;
            }

            if (g_shared_wayland.registry)
            {
                wl_registry_destroy(g_shared_wayland.registry);
                g_shared_wayland.registry = nullptr;
            }

            if (g_shared_wayland.display)
            {
                wl_display_disconnect(g_shared_wayland.display);
                g_shared_wayland.display = nullptr;
            }

            g_shared_wayland = shared_wayland_state_t{ };
        }

        void register_wayland_window(wayland_window_t* window)
        {
            if (!window)
                return;

            if (std::find(g_shared_wayland.windows.begin(), g_shared_wayland.windows.end(), window) == g_shared_wayland.windows.end())
                g_shared_wayland.windows.push_back(window);
        }

        void unregister_wayland_window(wayland_window_t* window)
        {
            std::erase(g_shared_wayland.windows, window);
        }

        void mark_all_wayland_windows_should_close() noexcept
        {
            for (wayland_window_t* window : g_shared_wayland.windows)
            {
                if (window)
                    window->set_should_close(true);
            }
        }

        [[nodiscard]] bool check_wayland_display_error(wl_display* display, std::string_view context)
        {
            if (!display)
                return false;

            const int error_code{ wl_display_get_error(display) };
            if (error_code == 0)
                return false;

            const wl_interface* interface{ nullptr };
            uint32_t object_id{ 0 };
            const uint32_t protocol_code{ wl_display_get_protocol_error(display, &interface, &object_id) };
            if (protocol_code != 0)
            {
                const char* interface_name{ interface ? interface->name : "unknown" };
                LOG_CORE_ERROR(
                    "Wayland protocol error in {}: interface={} object_id={} code={}",
                    context,
                    interface_name,
                    object_id,
                    protocol_code
                );
            }
            else
            {
                LOG_CORE_ERROR("Wayland display error in {}: errno={} ({})",
                               context,
                               error_code,
                               std::strerror(error_code));
            }

            return true;
        }

        void xdg_surface_configure(void* data, xdg_surface* xdg_surface, uint32_t serial)
        {
            auto* win = static_cast<wayland_window_t *>(data);

            xdg_surface_ack_configure(xdg_surface, serial);
            win->apply_pending_configure();
        }

        constexpr xdg_surface_listener xdg_surface_listener{ .configure = xdg_surface_configure };

        void xdg_toplevel_configure(void* data, xdg_toplevel*, const int32_t width, const int32_t height,
                                    wl_array* states)
        {
            auto* win{ static_cast<wayland_window_t *>(data) };

            bool fullscreen{ false };
            bool maximized{ false };
            bool resizing{ false };
            bool activated{ false };

            if (states && states->data)
            {
                const auto* state_values = static_cast<const uint32_t*>(states->data);
                const size_t count = states->size / sizeof(uint32_t);

                for (size_t i = 0; i < count; ++i)
                {
                    switch (state_values[i])
                    {
                        case XDG_TOPLEVEL_STATE_FULLSCREEN:
                            fullscreen = true;
                            break;
                        case XDG_TOPLEVEL_STATE_MAXIMIZED:
                            maximized = true;
                            break;
                        case XDG_TOPLEVEL_STATE_RESIZING:
                            resizing = true;
                            break;
                        case XDG_TOPLEVEL_STATE_ACTIVATED:
                            activated = true;
                            break;
                        default:
                            break;
                    }
                }
            }

            win->set_wayland_fullscreen_state(fullscreen);
            win->set_wayland_maximized_state(maximized);
            win->set_wayland_resizing_state(resizing);
            win->set_wayland_focused_state(activated);
            if (width > 0 && height > 0)
            {
                win->set_pending_width(static_cast<uint32_t>(width));
                win->set_pending_height(static_cast<uint32_t>(height));
            }

            win->set_pending_focus(activated);
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

        void surface_frame_done(void* data, wl_callback* callback, uint32_t time)
        {
            auto* win = static_cast<wayland_window_t*>(data);
            (void)time;

            if (callback)
                wl_callback_destroy(callback);

            if (win)
            {
                win->set_frame_callback(nullptr);
                win->set_ready_for_present(true);
            }
        }

        constexpr wl_callback_listener surface_frame_listener{
            .done = surface_frame_done
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
            .enter = [](void* data, wl_keyboard*, [[maybe_unused]] uint32_t serial, wl_surface*,
                        [[maybe_unused]] wl_array* keys) {
                auto* win = static_cast<wayland_window_t *>(data);
                if (!win->is_focused())
                {
                    win->set_wayland_focused_state(true);
                    win->on_window_focus_changed(events::window_focused_t{ true });
                }
            },
            .leave = [](void* data, wl_keyboard*, [[maybe_unused]] uint32_t serial, wl_surface*) {
                auto* win = static_cast<wayland_window_t *>(data);
                if (win->is_focused())
                {
                    win->set_wayland_focused_state(false);
                    win->on_window_focus_changed(events::window_focused_t{ false });
                }

                // Stop synthetic repeat immediately on keyboard focus loss.
                win->set_repeat_state_active(false);
                win->set_repeat_state_key(input::key_code::unknown);
            },
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

        void toplevel_decoration_configure(void* data,
                                   zxdg_toplevel_decoration_v1*,
                                   const uint32_t mode)
        {
            auto* win = static_cast<wayland_window_t*>(data);

            switch (mode)
            {
                case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
                    win->set_server_side_decorations(true);
                    break;

                case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
                    win->set_server_side_decorations(false);
                    break;

                default:
                    LOG_CORE_WARN("Wayland decoration mode: UNKNOWN ({})", mode);
                    win->set_server_side_decorations(false);
                    break;
            }
        }

        constexpr zxdg_toplevel_decoration_v1_listener toplevel_decoration_listener{
            .configure = toplevel_decoration_configure
        };

#ifdef CARROT_HAS_LIBDECOR
        void libdecor_error(libdecor*, const libdecor_error error, const char* message)
        {
            LOG_CORE_ERROR("libdecor error {}: {}", static_cast<int>(error), message ? message : "unknown");
        }

        libdecor_interface libdecor_listener{
            .error = libdecor_error,
            .reserved0 = nullptr,
            .reserved1 = nullptr,
            .reserved2 = nullptr,
            .reserved3 = nullptr,
            .reserved4 = nullptr,
            .reserved5 = nullptr,
            .reserved6 = nullptr,
            .reserved7 = nullptr,
            .reserved8 = nullptr,
            .reserved9 = nullptr,
        };

        void handle_libdecor_frame_configure(libdecor_frame* frame,
                                             libdecor_configuration* configuration,
                                             void* user_data)
        {
            auto* win = static_cast<wayland_window_t*>(user_data);

            int width{ static_cast<int>(win->get_width()) };
            int height{ static_cast<int>(win->get_height()) };
            (void)libdecor_configuration_get_content_size(configuration, frame, &width, &height);

            enum libdecor_window_state window_state{ LIBDECOR_WINDOW_STATE_NONE };
            const bool has_window_state{
                libdecor_configuration_get_window_state(configuration, &window_state)
            };

            win->set_pending_width(static_cast<uint32_t>(std::max(width, 1)));
            win->set_pending_height(static_cast<uint32_t>(std::max(height, 1)));
            win->set_wayland_fullscreen_state((window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) != 0);
            win->set_wayland_maximized_state((window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED) != 0);
            win->set_wayland_focused_state(has_window_state && (window_state & LIBDECOR_WINDOW_STATE_ACTIVE) != 0);
            win->set_pending_focus(win->is_focused());
            win->set_configure_pending(true);

            libdecor_state* state{
                libdecor_state_new(static_cast<int>(win->get_pending_width()),
                                   static_cast<int>(win->get_pending_height()))
            };
            if (!state)
            {
                LOG_CORE_ERROR("Failed to allocate libdecor state");
                return;
            }

            libdecor_frame_commit(frame, state, configuration);
            libdecor_state_free(state);
            win->apply_pending_configure();
        }

        void handle_libdecor_frame_close(libdecor_frame*, void* user_data)
        {
            auto* win = static_cast<wayland_window_t*>(user_data);
            win->set_should_close(true);
        }

        void handle_libdecor_frame_commit(libdecor_frame*, void* user_data)
        {
            auto* win = static_cast<wayland_window_t*>(user_data);
            if (!win->get_wl_surface())
                return;

            wl_surface_commit(win->get_wl_surface());
            if (win->get_wl_display())
                wl_display_flush(win->get_wl_display());
        }

        libdecor_frame_interface libdecor_frame_listener{
            .configure = handle_libdecor_frame_configure,
            .close = handle_libdecor_frame_close,
            .commit = handle_libdecor_frame_commit,
            .dismiss_popup = nullptr,
            .reserved0 = nullptr,
            .reserved1 = nullptr,
            .reserved2 = nullptr,
            .reserved3 = nullptr,
            .reserved4 = nullptr,
            .reserved5 = nullptr,
            .reserved6 = nullptr,
            .reserved7 = nullptr,
            .reserved8 = nullptr,
            .reserved9 = nullptr,
        };
#endif

        void shared_registry_global(void*, wl_registry* registry, const uint32_t name,
                                    const char* interface, const uint32_t version) noexcept
        {
            if (std::strcmp(interface, wl_compositor_interface.name) == 0)
            {
                g_shared_wayland.compositor = static_cast<wl_compositor*>(
                    wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u))
                );
            }
            else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0)
            {
                g_shared_wayland.xdg_wm_base = static_cast<xdg_wm_base*>(
                    wl_registry_bind(registry, name, &xdg_wm_base_interface, 1)
                );
                xdg_wm_base_add_listener(g_shared_wayland.xdg_wm_base, &xdg_wm_base_listener, nullptr);
            }
            else if (std::strcmp(interface, wl_seat_interface.name) == 0)
            {
                g_shared_wayland.seat_name = name;
                g_shared_wayland.seat_version = std::min(version, 4u);
            }
            else if (std::strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
            {
                g_shared_wayland.decoration_manager = static_cast<zxdg_decoration_manager_v1*>(
                    wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1)
                );
            }
        }

        void shared_registry_global_remove(void*, wl_registry*, uint32_t) noexcept {}

        constexpr wl_registry_listener shared_registry_listener{
            .global = shared_registry_global,
            .global_remove = shared_registry_global_remove,
        };
    } // anonymous

    void wayland_window_t::begin_poll_cycle() noexcept
    {
        ++g_shared_wayland.poll_generation;
    }

    wayland_window_t::wayland_window_t(const uint32_t width, const uint32_t height,
                                       const std::string_view title) noexcept
    {
        _width = width;
        _height = height;
        if (!acquire_shared_wayland_state())
            return;

        register_wayland_window(this);

        if (g_shared_wayland.windows.size() == 1 &&
            (g_shared_wayland.compositor == nullptr || g_shared_wayland.xdg_wm_base == nullptr))
        {
            wl_registry_add_listener(g_shared_wayland.registry, &shared_registry_listener, nullptr);
            wl_display_roundtrip(g_shared_wayland.display);
        }

        _display = g_shared_wayland.display;
        _compositor = g_shared_wayland.compositor;
        _xdg_wm_base = g_shared_wayland.xdg_wm_base;
        _decoration_manager = g_shared_wayland.decoration_manager;

        if (!_compositor)
            return;

        _surface = wl_compositor_create_surface(_compositor);

        if (g_shared_wayland.seat_name != 0 && g_shared_wayland.registry)
        {
            _seat = static_cast<wl_seat*>(
                wl_registry_bind(g_shared_wayland.registry, g_shared_wayland.seat_name, &wl_seat_interface,
                                 g_shared_wayland.seat_version)
            );
            if (_seat)
                wl_seat_add_listener(_seat, &seat_listener, this);
        }
#ifdef CARROT_HAS_LIBDECOR
        if (!g_shared_wayland.libdecor_context)
            g_shared_wayland.libdecor_context = libdecor_new(_display, &libdecor_listener);

        _libdecor_context = g_shared_wayland.libdecor_context;
        if (_libdecor_context)
        {
            _libdecor_frame = libdecor_decorate(_libdecor_context, _surface, &libdecor_frame_listener, this);
            if (_libdecor_frame)
            {
                _using_libdecor = true;
                libdecor_frame_set_app_id(_libdecor_frame, "bunnysoft.carrot");
                libdecor_frame_set_title(_libdecor_frame, std::string(title).c_str());
                libdecor_frame_map(_libdecor_frame);
                wl_display_roundtrip(_display);
            }
            else
            {
                LOG_CORE_WARN("libdecor was available but failed to decorate the Wayland surface; falling back to xdg-shell");
                _libdecor_context = nullptr;
            }
        }
#endif

        if (!_using_libdecor)
        {
            if (!_xdg_wm_base)
                return;

            _xdg_surface = xdg_wm_base_get_xdg_surface(_xdg_wm_base, _surface);
            xdg_surface_add_listener(_xdg_surface, &xdg_surface_listener, this);

            _xdg_toplevel = xdg_surface_get_toplevel(_xdg_surface);
            xdg_toplevel_add_listener(_xdg_toplevel, &xdg_toplevel_listener, this);

            if (_decoration_manager)
            {
                _toplevel_decoration =
                    zxdg_decoration_manager_v1_get_toplevel_decoration(_decoration_manager, _xdg_toplevel);

                zxdg_toplevel_decoration_v1_add_listener(
                    _toplevel_decoration,
                    &toplevel_decoration_listener,
                    this
                );

                zxdg_toplevel_decoration_v1_set_mode(
                    _toplevel_decoration,
                    ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
                );
            }

            xdg_toplevel_set_title(_xdg_toplevel, std::string(title).c_str());

            wl_surface_commit(_surface);
            wl_display_roundtrip(_display);
        }
    }

    wayland_window_t::~wayland_window_t() noexcept
    {
        if (_keyboard) wl_keyboard_destroy(_keyboard);
        if (_pointer) wl_pointer_destroy(_pointer);
        if (_frame_callback) wl_callback_destroy(_frame_callback);

        if (_xkb_state) xkb_state_unref(_xkb_state);
        if (_xkb_keymap) xkb_keymap_unref(_xkb_keymap);
        if (_xkb_context) xkb_context_unref(_xkb_context);

        if (_toplevel_decoration) zxdg_toplevel_decoration_v1_destroy(_toplevel_decoration);

#ifdef CARROT_HAS_LIBDECOR
        if (_libdecor_frame) libdecor_frame_unref(_libdecor_frame);
#endif

        if (_seat) wl_seat_destroy(_seat);
        if (_xdg_toplevel) xdg_toplevel_destroy(_xdg_toplevel);
        if (_xdg_surface) xdg_surface_destroy(_xdg_surface);
        if (_surface) wl_surface_destroy(_surface);
        unregister_wayland_window(this);
        release_shared_wayland_state();
    }

    void wayland_window_t::poll_events() noexcept
    {
        if (!_display) return;

        if (g_shared_wayland.last_pumped_generation != g_shared_wayland.poll_generation)
        {
            g_shared_wayland.last_pumped_generation = g_shared_wayland.poll_generation;

            // 1. Drain already-queued events first.
            if (wl_display_dispatch_pending(_display) < 0)
            {
                if (check_wayland_display_error(_display, "dispatch_pending(initial)"))
                    mark_all_wayland_windows_should_close();
                return;
            }

#ifdef CARROT_HAS_LIBDECOR
            if (g_shared_wayland.libdecor_context)
                libdecor_dispatch(g_shared_wayland.libdecor_context, 0);
#endif
            wl_display_flush(_display);

            // 2. Non-blocking read/dispatch of new compositor events.
            while (wl_display_prepare_read(_display) != 0)
            {
                if (check_wayland_display_error(_display, "prepare_read"))
                {
                    mark_all_wayland_windows_should_close();
                    return;
                }

                if (wl_display_dispatch_pending(_display) < 0)
                {
                    if (check_wayland_display_error(_display, "dispatch_pending(prepare_read loop)"))
                        mark_all_wayland_windows_should_close();
                    return;
                }
            }

            pollfd pfd{ };
            pfd.fd = wl_display_get_fd(_display);
            pfd.events = POLLIN;

            const int poll_result{ poll(&pfd, 1, 0) };
            if (poll_result > 0 && (pfd.revents & POLLIN))
            {
                if (wl_display_read_events(_display) == 0)
                {
                    if (wl_display_dispatch_pending(_display) < 0)
                    {
                        if (check_wayland_display_error(_display, "dispatch_pending(after read_events)"))
                            mark_all_wayland_windows_should_close();
                        return;
                    }

#ifdef CARROT_HAS_LIBDECOR
                    if (g_shared_wayland.libdecor_context)
                        libdecor_dispatch(g_shared_wayland.libdecor_context, 0);
#endif
                }
                else
                {
                    wl_display_cancel_read(_display);
                    if (check_wayland_display_error(_display, "read_events"))
                        mark_all_wayland_windows_should_close();
                    return;
                }
            }
            else
            {
                wl_display_cancel_read(_display);
                if (poll_result < 0 && errno != EINTR)
                    LOG_CORE_WARN("Wayland poll returned error: errno={} ({})", errno, std::strerror(errno));
                if (check_wayland_display_error(_display, "poll/cancel_read"))
                {
                    mark_all_wayland_windows_should_close();
                    return;
                }
            }
        }

        // 3. Generate synthetic repeats (only if we have an active repeating key)
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

    void wayland_window_t::set_title(std::string_view title) noexcept
    {
        if (_using_libdecor)
        {
#ifdef CARROT_HAS_LIBDECOR
            if (!_libdecor_frame)
                return;

            libdecor_frame_set_title(_libdecor_frame, std::string(title).c_str());
            if (_display)
                wl_display_flush(_display);
#endif
            return;
        }

        if (!_xdg_toplevel)
            return;

        xdg_toplevel_set_title(_xdg_toplevel, std::string(title).c_str());
        wl_surface_commit(_surface);
    }

    void wayland_window_t::minimize() noexcept
    {
        if (_using_libdecor)
        {
#ifdef CARROT_HAS_LIBDECOR
            if (!_libdecor_frame)
                return;

            libdecor_frame_set_minimized(_libdecor_frame);
            _is_minimized = true;
            if (_display)
                wl_display_flush(_display);
#endif
            return;
        }

        if (!_xdg_toplevel || !_surface)
            return;

        xdg_toplevel_set_minimized(_xdg_toplevel);
        wl_surface_commit(_surface);
    }

    void wayland_window_t::maximize() noexcept
    {
        if (_using_libdecor)
        {
#ifdef CARROT_HAS_LIBDECOR
            if (!_libdecor_frame)
                return;

            libdecor_frame_set_maximized(_libdecor_frame);
            if (_display)
                wl_display_flush(_display);
#endif
            return;
        }

        if (!_xdg_toplevel || !_surface)
            return;

        xdg_toplevel_set_maximized(_xdg_toplevel);
        wl_surface_commit(_surface);
    }

    void wayland_window_t::restore() noexcept
    {
        if (_using_libdecor)
        {
#ifdef CARROT_HAS_LIBDECOR
            if (!_libdecor_frame)
                return;

            libdecor_frame_unset_fullscreen(_libdecor_frame);
            libdecor_frame_unset_maximized(_libdecor_frame);
            _is_minimized = false;
            if (_display)
                wl_display_flush(_display);
#endif
            return;
        }

        if (!_xdg_toplevel || !_surface)
            return;

        xdg_toplevel_unset_fullscreen(_xdg_toplevel);
        xdg_toplevel_unset_maximized(_xdg_toplevel);

        wl_surface_commit(_surface);
    }

    void wayland_window_t::request_focus() noexcept
    {
        if (!_surface || !_display)
            return;

        // Wayland compositors own activation policy; clients generally cannot force focus.
        // Best effort here is to commit/flush surface state and let compositor policy decide.
        wl_surface_commit(_surface);
        wl_display_flush(_display);
    }

    void wayland_window_t::prepare_for_present() noexcept
    {
        if (!_surface || !_display || _frame_callback != nullptr || !_ready_for_present)
            return;

        _frame_callback = wl_surface_frame(_surface);
        if (!_frame_callback)
            return;

        wl_callback_add_listener(_frame_callback, &surface_frame_listener, this);
        _ready_for_present = false;
    }

    native_window_handle_t wayland_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ nullptr };
        handle.wayland_t.display = _display;
        handle.wayland_t.surface = _surface;

        return handle;
    }

    void wayland_window_t::set_fullscreen(const bool fullscreen) noexcept
    {
        if (_using_libdecor)
        {
#ifdef CARROT_HAS_LIBDECOR
            if (!_libdecor_frame)
                return;

            if (fullscreen == _is_fullscreen)
                return;

            if (fullscreen)
                libdecor_frame_set_fullscreen(_libdecor_frame, nullptr);
            else
                libdecor_frame_unset_fullscreen(_libdecor_frame);

            _is_fullscreen = fullscreen;
            if (_display)
                wl_display_flush(_display);
#endif
            return;
        }

        if (!_xdg_toplevel || !_surface)
            return;

        // Wayland fullscreen state is compositor-driven and asynchronous.
        // Keep requests idempotent to avoid transition thrash.
        if (fullscreen == _is_fullscreen)
            return;

        if (fullscreen)
            xdg_toplevel_set_fullscreen(_xdg_toplevel, nullptr);
        else
            xdg_toplevel_unset_fullscreen(_xdg_toplevel);

        // Optimistically track requested state; compositor configure will reconcile actual state.
        _is_fullscreen = fullscreen;
        wl_surface_commit(_surface);
        if (_display)
            wl_display_flush(_display);
    }

    void wayland_window_t::apply_pending_configure() noexcept
    {
        if (!_configure_pending)
            return;

        const bool old_focused = _is_focused;
        const uint32_t old_width = _width;
        const uint32_t old_height = _height;


        if (_pending_width > 0 && _pending_height > 0)
        {
            _width = _pending_width;
            _height = _pending_height;
        }

        _configure_pending = false;

        if (_width != old_width || _height != old_height)
            on_window_resized(events::window_resized_t{ _width, _height });

        if (_pending_focus != old_focused)
            on_window_focus_changed(events::window_focused_t{ _is_focused });
    }
} // namespace carrot::core::platform
