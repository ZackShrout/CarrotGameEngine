//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Events/Events.h"

namespace carrot::scene {
    class scene_runtime_t;
}

namespace carrot::core {
    struct game_context_t;
    class game_runtime_t;

    class igame_state_t
    {
    public:
        explicit igame_state_t(game_runtime_t& runtime) noexcept;
        virtual ~igame_state_t() = default;

        virtual void enter() {}
        virtual void exit() {}
        virtual void tick(float delta_time) = 0;
        virtual void render_overlay() {}
        [[nodiscard]] virtual scene::scene_runtime_t* scene_runtime() noexcept { return nullptr; }
        [[nodiscard]] virtual const scene::scene_runtime_t* scene_runtime() const noexcept { return nullptr; }
        virtual void on_window_focus_changed(const events::window_focused_t& e) { (void)e; }
        virtual void on_key(const events::key_event_t& e) { (void)e; }
        virtual void on_mouse_moved(const events::mouse_moved_event_t& e) { (void)e; }
        virtual void on_mouse_button(const events::mouse_button_event_t& e) { (void)e; }
        virtual void on_mouse_scrolled(const events::mouse_scrolled_event_t& e) { (void)e; }

    protected:
        [[nodiscard]] game_runtime_t& runtime() noexcept { return _runtime; }
        [[nodiscard]] const game_runtime_t& runtime() const noexcept { return _runtime; }
        [[nodiscard]] game_context_t& game() noexcept;
        [[nodiscard]] const game_context_t& game() const noexcept;

    private:
        game_runtime_t& _runtime;
    };
} // namespace carrot::core
