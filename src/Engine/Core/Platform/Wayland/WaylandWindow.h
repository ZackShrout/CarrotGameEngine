//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Platform/Window.h"
#include "Input/KeyCodes.h"
#include "chlm/CarrotHLM.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;

namespace carrot::core::platform {
    struct key_repeat_state_t
    {
        input::key_code _key{ input::key_code::unknown };
        bool _active{ false };
        bool _first_repeat_sent{ false };
        uint32_t _last_time_ms{ 0 };
        uint32_t _delay_ms{ 500 };
        uint32_t _rate_ms{ 33 }; // ~30 Hz = 33 ms
    };

    class wayland_window_t final : public window_t
    {
    public:
        explicit wayland_window_t(uint32_t width, uint32_t height, std::string_view title) noexcept;
        ~wayland_window_t() noexcept override;

        void poll_events() noexcept override;
        void set_should_close(const bool should_close) noexcept override { _should_close = should_close; }

        void set_title(std::string_view title) noexcept override;
        void minimize() noexcept override;
        void maximize() noexcept override;
        void restore() noexcept override;
        void request_focus() noexcept override;

        [[nodiscard]] bool is_maximized() const noexcept override { return _is_maximized; }
        [[nodiscard]] bool is_minimized() const noexcept override { return _is_minimized; }
        [[nodiscard]] bool is_focused() const noexcept override { return _is_focused; }
        [[nodiscard]] native_window_handle_t get_native_handle() const noexcept override;

        void set_fullscreen(bool fullscreen) noexcept override;

        void apply_pending_configure() noexcept;

        [[nodiscard]] wl_display* get_wl_display() const noexcept { return _display; }
        [[nodiscard]] wl_surface* get_wl_surface() const noexcept { return _surface; }
        [[nodiscard]] wl_seat* get_wl_seat() const noexcept { return _seat; }
        [[nodiscard]] wl_keyboard* get_wl_keyboard() const noexcept { return _keyboard; }
        [[nodiscard]] wl_pointer* get_wl_pointer() const noexcept { return _pointer; }
        [[nodiscard]] xkb_context* get_xkb_context() const noexcept { return _xkb_context; }
        [[nodiscard]] xkb_keymap* get_xkb_keymap() const noexcept { return _xkb_keymap; }
        [[nodiscard]] xkb_state* get_xkb_state() const noexcept { return _xkb_state; }
        [[nodiscard]] uint32_t get_pending_width() const noexcept { return _pending_width; }
        [[nodiscard]] uint32_t get_pending_height() const noexcept { return _pending_height; }
        [[nodiscard]] bool get_configure_pending() const noexcept { return _configure_pending; }
        [[nodiscard]] chlm::float2 get_last_mouse_pos() const noexcept { return _last_mouse_pos; }
        [[nodiscard]] uint8_t get_keyboard_mods() const noexcept { return _keyboard_mods; }
        [[nodiscard]] key_repeat_state_t get_repeat_state() const noexcept { return _repeat_state; }
        [[nodiscard]] bool get_repeat_enabled() const noexcept { return _repeat_enabled; }

        [[nodiscard]] zxdg_decoration_manager_v1* get_decoration_manager() const noexcept { return _decoration_manager; }
        [[nodiscard]] zxdg_toplevel_decoration_v1* get_toplevel_decoration() const noexcept { return _toplevel_decoration; }
        [[nodiscard]] bool has_server_side_decorations() const noexcept { return _server_side_decorations; }

        void set_compositor(wl_compositor* c) noexcept { _compositor = c; }
        void set_xdg_wm_base(xdg_wm_base* base) noexcept { _xdg_wm_base = base; }
        void set_seat(wl_seat* seat) noexcept { _seat = seat; }
        void set_keyboard(wl_keyboard* keyboard) noexcept { _keyboard = keyboard; }
        void set_pointer(wl_pointer* pointer) noexcept { _pointer = pointer; }
        void set_xkb_context(xkb_context* context) noexcept { _xkb_context = context; }
        void set_xkb_keymap(xkb_keymap* keymap) noexcept { _xkb_keymap = keymap; }
        void set_xkb_state(xkb_state* state) noexcept { _xkb_state = state; }
        void set_current_width(const uint32_t width) noexcept { _width = width; }
        void set_current_height(const uint32_t height) noexcept { _height = height; }
        void set_pending_width(const uint32_t width) noexcept { _pending_width = width; }
        void set_pending_height(const uint32_t height) noexcept { _pending_height = height; }
        void set_pending_focus(const bool activate) noexcept { _pending_focus = activate; }
        void set_configure_pending(const bool configure) noexcept { _configure_pending = configure; }
        void set_last_mouse_pos(const chlm::float2& pos) noexcept { _last_mouse_pos = pos; }
        void set_keyboard_mods(const uint8_t mods) noexcept { _keyboard_mods = mods; }
        void set_repeat_enabled(const bool enabled) noexcept { _repeat_enabled = enabled; }
        void set_repeat_state_key(const input::key_code code) noexcept { _repeat_state._key = code; }
        void set_repeat_state_active(const bool active) noexcept { _repeat_state._active = active; }
        void set_repeat_state_first_sent(const bool sent) noexcept { _repeat_state._first_repeat_sent = sent; }
        void set_repeat_state_last_time(const uint32_t value) noexcept { _repeat_state._last_time_ms = value; }
        void set_repeat_state_delay(const uint32_t value) noexcept { _repeat_state._delay_ms = value; }
        void set_repeat_state_rate(const uint32_t value) noexcept { _repeat_state._rate_ms = value; }

        void set_decoration_manager(zxdg_decoration_manager_v1* mgr) noexcept { _decoration_manager = mgr; }
        void set_toplevel_decoration(zxdg_toplevel_decoration_v1* deco) noexcept { _toplevel_decoration = deco; }
        void set_server_side_decorations(bool value) noexcept { _server_side_decorations = value; }

        void set_wayland_fullscreen_state(const bool value) noexcept { _is_fullscreen = value; }
        void set_wayland_maximized_state(const bool value) noexcept { _is_maximized = value; }
        void set_wayland_focused_state(const bool value) noexcept { _is_focused = value; }
        void set_wayland_resizing_state(const bool value) noexcept { _is_resizing = value; }
        void set_wayland_minimized_state(const bool value) noexcept { _is_minimized = value; }

    private:
        wl_display* _display{ nullptr };
        wl_compositor* _compositor{ nullptr };
        wl_surface* _surface{ nullptr };
        xdg_wm_base* _xdg_wm_base{ nullptr };
        xdg_surface* _xdg_surface{ nullptr };
        xdg_toplevel* _xdg_toplevel{ nullptr };
        wl_seat* _seat{ nullptr };
        wl_keyboard* _keyboard{ nullptr };
        wl_pointer* _pointer{ nullptr };
        xkb_context* _xkb_context{ nullptr };
        xkb_keymap* _xkb_keymap{ nullptr };
        xkb_state* _xkb_state{ nullptr };

        uint32_t _pending_width{ 0 };
        uint32_t _pending_height{ 0 };
        bool _pending_focus{ false };
        bool _configure_pending{ false };
        bool _is_maximized{ false };
        bool _is_focused{ false };
        bool _is_resizing{ false };

        chlm::float2 _last_mouse_pos{ 0.f, 0.f };
        uint8_t _keyboard_mods{ 0 };
        key_repeat_state_t _repeat_state;
        bool _repeat_enabled{ true };

        zxdg_decoration_manager_v1* _decoration_manager{ nullptr };
        zxdg_toplevel_decoration_v1* _toplevel_decoration{ nullptr };
        bool _server_side_decorations{ false };
    };
} // namespace carrot::core::platform
