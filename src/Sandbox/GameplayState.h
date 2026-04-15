//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "GameplayRuntimeState.h"

#include <CarrotEngine.h>

namespace sandbox {
    class gameplay_state_t final
            : public carrot::core::igame_state_t
              , public carrot::scene::scene_runtime_listener_t
    {
    public:
        gameplay_state_t(carrot::core::game_runtime_t& runtime,
                         carrot::input::gameplay_input_router_t& input) noexcept
            : igame_state_t(runtime), _input(input) {}

        void enter() override;
        void tick(float delta_time) override;
        [[nodiscard]] carrot::scene::scene_runtime_t* scene_runtime() noexcept override { return &_scene_runtime; }

        [[nodiscard]] const carrot::scene::scene_runtime_t* scene_runtime() const noexcept override
        {
            return &_scene_runtime;
        }

        void on_window_focus_changed(const carrot::events::window_focused_t& e) override;
        void on_key(const carrot::events::key_event_t& e) override;
        void before_scene_change(carrot::core::game_context_t& game,
                                 const carrot::scene::scene_runtime_context_t* current_context,
                                 std::string_view next_scene_id,
                                 std::string_view next_spawn_marker) override;
        void after_scene_change(carrot::core::game_context_t& game,
                                const carrot::scene::scene_runtime_context_t& current_context) override;

    private:
        [[nodiscard]] carrot::scene::scene_load_options_t make_scene_load_options(
            std::string_view spawn_marker_override = { }) noexcept;
        void prepare_for_scene_change(const carrot::scene::scene_runtime_context_t* current_context) noexcept;
        void finalize_scene_change(const carrot::scene::scene_runtime_context_t& current_context) noexcept;
        void consume_pending_runtime_events() noexcept;
        void handle_trigger_event(const carrot::world::trigger_event_t& event) noexcept;
        void toggle_map_collision_debug() noexcept;
        void toggle_object_collision_debug() noexcept;
        void toggle_trigger_volume_debug() noexcept;
        bool load_scene(std::string_view scene_id, std::string_view spawn_marker = { });
        bool transition_scene(const carrot::scene::scene_transition_request_t& request);

        carrot::input::gameplay_input_router_t& _input;
        carrot::world::player_controller_t _player_controller;
        carrot::world::interaction_controller_t _interaction_controller;
        carrot::scene::scene_runtime_t _scene_runtime;
        gameplay_runtime_state_t _runtime_state;
        carrot::world::trigger_monitor_t _trigger_monitor;
    };
} // namespace sandbox
