//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "SandboxInteractionController.h"

#include <CarrotEngine.h>

namespace sandbox {
    class sandbox_t : public carrot::core::ce_application_t
    {
        void configure_fallback_input_actions();
        void configure_default_input_actions();
        void apply_scene_camera_defaults() noexcept;
        void center_camera_on_initial_target() noexcept;
        void update_camera_follow(float delta_time) noexcept;
        void refresh_scene_bindings() noexcept;
        void refresh_scene_music();
        bool load_scene(std::string_view scene_id, std::string_view spawn_marker = {});

        carrot::core::game_context_t* _game{ nullptr };
        carrot::input::input_action_map_t _actions;
        carrot::world::player_controller_t _player_controller;
        sandbox_interaction_controller_t _interaction_controller;
        std::string _current_scene_id;
        std::string _current_spawn_marker;
        carrot::assets::scene_camera_follow_mode_t _camera_follow_mode{
            carrot::assets::scene_camera_follow_mode_t::player
        };
        carrot::assets::scene_camera_initial_target_policy_t _camera_initial_target_policy{
            carrot::assets::scene_camera_initial_target_policy_t::player
        };
        chlm::float2 _camera_dead_zone_size_world{ 0.f, 0.f };
        float _camera_follow_smoothing{ 0.f };

        void start(carrot::core::game_context_t& game) override;

        void on_tick([[maybe_unused]] float delta_time) override;
        void on_key(const carrot::events::key_event_t& e) override;
        void on_mouse_moved(const carrot::events::mouse_moved_event_t& e) override;
        void on_mouse_button(const carrot::events::mouse_button_event_t& e) override;
        void on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e) override;
    };
} // namespace sandbox
