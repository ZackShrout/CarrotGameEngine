//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "CoreDefines.h"
#include "Events/Events.h"
#include "Window/Window.h"

#include <memory>

namespace carrot::core {
    struct game_context_t;
    class igame_state_t;

    class game_runtime_t
    {
    public:
        DISABLE_COPY_AND_MOVE(game_runtime_t)

        explicit game_runtime_t(game_context_t& game) noexcept;
        virtual ~game_runtime_t();

        virtual void start() {}
        virtual void tick(float delta_time);
        virtual void on_window_focus_changed(const events::window_focused_t& e);
        virtual void on_key(const events::key_event_t& e);
        virtual void on_mouse_moved(const events::mouse_moved_event_t& e);
        virtual void on_mouse_button(const events::mouse_button_event_t& e);
        virtual void on_mouse_scrolled(const events::mouse_scrolled_event_t& e);

        void set_active_state(std::unique_ptr<igame_state_t> state);
        void clear_active_state();

        [[nodiscard]] igame_state_t* active_state() noexcept { return _active_state.get(); }
        [[nodiscard]] const igame_state_t* active_state() const noexcept { return _active_state.get(); }
        [[nodiscard]] game_context_t& game() noexcept { return _game; }
        [[nodiscard]] const game_context_t& game() const noexcept { return _game; }

        [[nodiscard]] static bool is_fullscreen() { return window::is_fullscreen(); }
        static void set_fullscreen(bool fullscreen) noexcept { window::set_fullscreen(fullscreen); }
        static void quit_application() { window::set_should_close(true); }

    protected:
        game_runtime_t() = delete;

    private:
        game_context_t& _game;
        std::unique_ptr<igame_state_t> _active_state;
    };
} // namespace carrot::core
