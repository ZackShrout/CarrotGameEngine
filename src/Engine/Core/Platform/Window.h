//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Platform.h"
#include "Events/Events.h"
#include "Utils/MulticastDelegate.h"


namespace carrot::core::platform {
    class window_t
    {
    public:
        virtual ~window_t() = default;

        virtual void poll_events() noexcept = 0;
        virtual void set_should_close(bool should_close) noexcept = 0;

        virtual void set_title(std::string_view) noexcept {}
        virtual void minimize() noexcept {}
        virtual void maximize() noexcept {}
        virtual void restore() noexcept {}

        [[nodiscard]] virtual bool is_maximized() const noexcept { return false; }
        [[nodiscard]] virtual bool is_focused() const noexcept { return true; }

        [[nodiscard]] virtual bool should_close() const noexcept { return _should_close; }
        [[nodiscard]] virtual uint32_t get_width()  const noexcept { return _width; }
        [[nodiscard]] virtual uint32_t get_height() const noexcept { return _height; }
        [[nodiscard]] virtual bool is_minimized() const noexcept { return _is_minimized; }
        [[nodiscard]] virtual native_window_handle_t get_native_handle() const noexcept = 0;

        [[nodiscard]] virtual bool is_fullscreen() const noexcept { return _is_fullscreen; }
        virtual void set_fullscreen(const bool fullscreen) noexcept { _is_fullscreen = fullscreen; }


        // ───────────────────────────────────────────────
        // Window events
        // ───────────────────────────────────────────────
        DECLARE_MULTICAST_DELEGATE(on_window_resized_sig, const events::window_resized_t&);
        DECLARE_MULTICAST_DELEGATE(on_window_closed_sig, const events::window_closed_t&);
        DECLARE_MULTICAST_DELEGATE(on_window_focused_sig, const events::window_focused_t&);

        on_window_resized_sig _on_window_resized;
        on_window_closed_sig _on_window_closed;
        on_window_focused_sig _on_window_focus_changed;

        virtual void on_window_resized(const events::window_resized_t& e) { _on_window_resized.broadcast(e); }
        virtual void on_window_closed(const events::window_closed_t& e) { _on_window_closed.broadcast(e); }

        virtual void on_window_focus_changed(const events::window_focused_t& e)
        {
            _on_window_focus_changed.broadcast(e);
        }

        // ───────────────────────────────────────────────
        // Input events
        // ───────────────────────────────────────────────

        DECLARE_MULTICAST_DELEGATE(on_key_sig, const events::key_event_t&);
        DECLARE_MULTICAST_DELEGATE(on_mouse_button_sig, const events::mouse_button_event_t&);
        DECLARE_MULTICAST_DELEGATE(on_mouse_moved_sig, const events::mouse_moved_event_t&);
        DECLARE_MULTICAST_DELEGATE(on_mouse_scrolled_sig, const events::mouse_scrolled_event_t&);

        on_key_sig _on_key;
        on_mouse_button_sig _on_mouse_button;
        on_mouse_moved_sig _on_mouse_moved;
        on_mouse_scrolled_sig _on_mouse_scrolled;

        void on_key(const events::key_event_t& e) const { _on_key.broadcast(e); }
        void on_mouse_button(const events::mouse_button_event_t& e) const { _on_mouse_button.broadcast(e); }
        void on_mouse_moved(const events::mouse_moved_event_t& e) const { _on_mouse_moved.broadcast(e); }
        void on_mouse_scrolled(const events::mouse_scrolled_event_t& e) const { _on_mouse_scrolled.broadcast(e); }

    protected:
        bool _should_close{ false };
        bool _is_minimized{ false };
        bool _is_fullscreen{ false };
        uint32_t _width{ 0 }, _height{ 0 };
    };

} // namespace carrot::core::platform
