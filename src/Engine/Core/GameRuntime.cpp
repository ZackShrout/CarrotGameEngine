//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameRuntime.h"

#include "GameState.h"

namespace carrot::core {
    game_runtime_t::game_runtime_t(game_context_t& game) noexcept
        : _game(game)
    {
    }

    game_runtime_t::~game_runtime_t()
    {
        clear_active_state();
    }

    void game_runtime_t::tick(const float delta_time)
    {
        if (_active_state)
            _active_state->tick(delta_time);
    }

    void game_runtime_t::on_window_focus_changed(const events::window_focused_t& e)
    {
        if (_active_state)
            _active_state->on_window_focus_changed(e);
    }

    void game_runtime_t::on_key(const events::key_event_t& e)
    {
        if (_active_state)
            _active_state->on_key(e);
    }

    void game_runtime_t::on_mouse_moved(const events::mouse_moved_event_t& e)
    {
        if (_active_state)
            _active_state->on_mouse_moved(e);
    }

    void game_runtime_t::on_mouse_button(const events::mouse_button_event_t& e)
    {
        if (_active_state)
            _active_state->on_mouse_button(e);
    }

    void game_runtime_t::on_mouse_scrolled(const events::mouse_scrolled_event_t& e)
    {
        if (_active_state)
            _active_state->on_mouse_scrolled(e);
    }

    void game_runtime_t::set_active_state(std::unique_ptr<igame_state_t> state)
    {
        if (_active_state)
            _active_state->exit();

        _active_state = std::move(state);

        if (_active_state)
            _active_state->enter();
    }

    void game_runtime_t::clear_active_state()
    {
        if (_active_state)
            _active_state->exit();

        _active_state.reset();
    }
} // namespace carrot::core
