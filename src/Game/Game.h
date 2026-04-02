//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

namespace sandbox {
    enum class facing_direction_t : uint8_t
    {
        down = 0,
        up,
        left,
        right
    };

    class sandbox_t : public carrot::core::ce_application_t
    {
        carrot::core::game_context_t* _game{ nullptr };
        bool _move_up{ false };
        bool _move_down{ false };
        bool _move_left{ false };
        bool _move_right{ false };
        float _player_move_speed{ 4.0f };
        facing_direction_t _facing_direction{ facing_direction_t::down };
        std::string _current_player_animation{ "idle_down" };

        void start(carrot::core::game_context_t& game) override;

        void on_tick([[maybe_unused]] float delta_time) override;
        void on_key(const carrot::events::key_event_t& e) override;
        void on_mouse_moved(const carrot::events::mouse_moved_event_t& e) override;
        void on_mouse_button(const carrot::events::mouse_button_event_t& e) override;
        void on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e) override;
    };
} // namespace sandbox
