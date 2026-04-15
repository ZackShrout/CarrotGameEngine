//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameRuntime.h"

#include "GameContext.h"
#include "GameState.h"
#include "Scene/Scene.h"

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
        {
            _active_state->tick(delta_time);
            if (scene::scene_runtime_t* runtime{ _active_state->scene_runtime() })
                runtime->advance_transition_overlay(delta_time);
        }
    }

    void game_runtime_t::render_overlay()
    {
        if (_active_state)
        {
            if (scene::scene_runtime_t* runtime{ _active_state->scene_runtime() })
                runtime->render_transition_overlay(_game);
            _active_state->render_overlay();
        }
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

    bool game_runtime_t::map_collision_debug_visible() const noexcept
    {
        return _game.world.collision_debug_view().show_map_collision;
    }

    bool game_runtime_t::object_collider_debug_visible() const noexcept
    {
        return _game.world.collision_debug_view().show_object_colliders;
    }

    bool game_runtime_t::trigger_volume_debug_visible() const noexcept
    {
        return _game.world.collision_debug_view().show_trigger_volumes;
    }

    void game_runtime_t::set_map_collision_debug_visible(const bool visible) noexcept
    {
        _game.world.collision_debug_view().show_map_collision = visible;
    }

    void game_runtime_t::set_object_collider_debug_visible(const bool visible) noexcept
    {
        _game.world.collision_debug_view().show_object_colliders = visible;
    }

    void game_runtime_t::set_trigger_volume_debug_visible(const bool visible) noexcept
    {
        _game.world.collision_debug_view().show_trigger_volumes = visible;
    }

    bool game_runtime_t::toggle_map_collision_debug() noexcept
    {
        const bool next_visible{ !map_collision_debug_visible() };
        set_map_collision_debug_visible(next_visible);
        return next_visible;
    }

    bool game_runtime_t::toggle_object_collider_debug() noexcept
    {
        const bool next_visible{ !object_collider_debug_visible() };
        set_object_collider_debug_visible(next_visible);
        return next_visible;
    }

    bool game_runtime_t::toggle_trigger_volume_debug() noexcept
    {
        const bool next_visible{ !trigger_volume_debug_visible() };
        set_trigger_volume_debug_visible(next_visible);
        return next_visible;
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
