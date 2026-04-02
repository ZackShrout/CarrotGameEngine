//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

#include "SandboxInteractionController.h"

namespace sandbox {
    class sandbox_t : public carrot::core::ce_application_t
    {
        carrot::core::game_context_t* _game{ nullptr };
        carrot::world::player_controller_t _player_controller;
        sandbox_interaction_controller_t _interaction_controller;

        void start(carrot::core::game_context_t& game) override;

        void on_tick([[maybe_unused]] float delta_time) override;
        void on_key(const carrot::events::key_event_t& e) override;
        void on_mouse_moved(const carrot::events::mouse_moved_event_t& e) override;
        void on_mouse_button(const carrot::events::mouse_button_event_t& e) override;
        void on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e) override;
    };
} // namespace sandbox
