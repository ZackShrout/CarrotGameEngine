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
        [[nodiscard]] virtual bool should_close() const noexcept = 0;
        virtual void set_should_close(bool should_close) noexcept = 0;;

        [[nodiscard]] virtual uint32_t get_width()  const noexcept = 0;
        [[nodiscard]] virtual uint32_t get_height() const noexcept = 0;

        [[nodiscard]] virtual native_window_handle_t get_native_handle() const noexcept = 0;

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
    };

} // namespace carrot::core::platform