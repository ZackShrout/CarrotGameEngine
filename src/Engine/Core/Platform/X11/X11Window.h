//
// Created by zshrout on 4/10/26.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Platform/Window.h"

#include <X11/Xlib.h>

#include <string>
#include <unordered_set>

namespace carrot::core::platform {
    class x11_window_t final : public window_t
    {
    public:
        explicit x11_window_t(uint32_t width, uint32_t height, std::string_view title) noexcept;
        ~x11_window_t() noexcept override;

        void poll_events() noexcept override;
        void set_should_close(bool should_close) noexcept override;
        void set_title(std::string_view title) noexcept override;
        void minimize() noexcept override;
        void maximize() noexcept override;
        void restore() noexcept override;
        void request_focus() noexcept override;

        [[nodiscard]] bool is_maximized() const noexcept override { return _is_maximized; }
        [[nodiscard]] bool is_focused() const noexcept override { return _is_focused; }
        [[nodiscard]] bool is_resizing() const noexcept override { return _is_resizing; }
        [[nodiscard]] native_window_handle_t get_native_handle() const noexcept override;

        void set_fullscreen(bool fullscreen) noexcept override;

        static void begin_poll_cycle() noexcept;
        void handle_event(const XEvent& event) noexcept;

        [[nodiscard]] Display* get_display() const noexcept { return _display; }
        [[nodiscard]] Window get_window() const noexcept { return _window; }
        [[nodiscard]] std::string_view get_title() const noexcept { return _title; }

    private:
        void apply_wm_state_atom(Atom state_atom, bool enabled) noexcept;
        void sync_wm_state() noexcept;
        void update_modifiers(unsigned int state) noexcept;
        void update_size(uint32_t width, uint32_t height) noexcept;
        void handle_key_press(const XKeyEvent& event) noexcept;
        void handle_key_release(const XKeyEvent& event) noexcept;
        void handle_button_press(const XButtonEvent& event) noexcept;
        void handle_button_release(const XButtonEvent& event) noexcept;
        void handle_motion(const XMotionEvent& event) noexcept;

        Display* _display{ nullptr };
        Window _window{ 0 };
        int _screen{ 0 };
        Atom _wm_delete_window{ 0 };
        std::string _title;
        uint8_t _mods{ 0 };
        chlm::float2 _last_mouse_position{ 0.f, 0.f };
        std::unordered_set<input::key_code> _pressed_keys;
        bool _is_maximized{ false };
        bool _is_focused{ false };
        bool _is_resizing{ false };
    };
} // namespace carrot::core::platform
