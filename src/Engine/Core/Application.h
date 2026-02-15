//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Common/CommonHeaders.h"
#include "Events/Events.h"
#include "Utils/MulticastDelegate.h"
#include "Window/Window.h"

namespace carrot::core {
    class ce_application_t
    {
    public:
        DISABLE_COPY_AND_MOVE(ce_application_t)

        virtual void start() {}

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

        virtual void on_key(const events::key_event_t& e) { _on_key.broadcast(e); }
        virtual void on_mouse_button(const events::mouse_button_event_t& e) { _on_mouse_button.broadcast(e); }
        virtual void on_mouse_moved(const events::mouse_moved_event_t& e) { _on_mouse_moved.broadcast(e); }
        virtual void on_mouse_scrolled(const events::mouse_scrolled_event_t& e) { _on_mouse_scrolled.broadcast(e); }

        // ───────────────────────────────────────────────
        // Tick event
        // ───────────────────────────────────────────────
        virtual void on_tick([[maybe_unused]] const float delta_time) {}

    protected:
        ce_application_t() noexcept = default;
        virtual ~ce_application_t() = default;

        static bool is_fullscreen() { return window::is_fullscreen(); }
        static void set_fullscreen(const bool fullscreen) noexcept { window::set_fullscreen(fullscreen); }
        static void quit_application() { window::set_should_close(true); }
    };
} // namespace carrot
