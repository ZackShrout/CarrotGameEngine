//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "ActionMap.h"
#include "ControllerManager.h"

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

namespace carrot::core {
    class ce_application_t;
    struct game_context_t;
}

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::world {
    class interaction_controller_t;
    class player_controller_t;
}

namespace carrot::input {
    enum class gameplay_input_routing_mode_t : std::uint8_t
    {
        single_player_auto = 0,
        local_multiplayer_fixed
    };

    struct player_input_assignment_t
    {
        bool receives_keyboard{ false };
        std::optional<uint32_t> gamepad_slot;
    };

    struct gameplay_input_routing_config_t
    {
        gameplay_input_routing_mode_t mode{ gameplay_input_routing_mode_t::single_player_auto };
        size_t player_count{ 1u };
        std::array<player_input_assignment_t, controller_manager_t::max_gamepad_slots> assignments{ };
    };

    struct player_input_context_t
    {
        size_t player_index{ 0u };
        player_input_assignment_t assignment{ };

        [[nodiscard]] bool receives_keyboard() const noexcept { return assignment.receives_keyboard; }
        [[nodiscard]] std::optional<uint32_t> gamepad_slot() const noexcept { return assignment.gamepad_slot; }
    };

    struct gameplay_input_profile_t
    {
        input_action_handle_t move_up{ };
        input_action_handle_t move_down{ };
        input_action_handle_t move_left{ };
        input_action_handle_t move_right{ };
        input_action_handle_t interact{ };
        input_action_handle_t quit{ };
        input_action_handle_t toggle_fullscreen{ };
        input_action_handle_t toggle_map_collision_debug{ };
        input_action_handle_t toggle_object_collision_debug{ };
        input_action_handle_t ui_up{ };
        input_action_handle_t ui_down{ };
        input_action_handle_t ui_left{ };
        input_action_handle_t ui_right{ };
        input_action_handle_t ui_accept{ };
        input_action_handle_t ui_cancel{ };
    };

    class gameplay_input_router_t
    {
    public:
        gameplay_input_router_t() noexcept;

        struct repeat_state_t
        {
            bool was_pressed{ false };
            float hold_time_seconds{ 0.f };
            float repeat_time_seconds{ 0.f };
        };

        [[nodiscard]] const input_action_map_t& actions() const noexcept { return _action_bindings; }
        [[nodiscard]] input_action_map_t& actions() noexcept
        {
            _bindings_dirty = true;
            return _action_bindings;
        }

        void configure_routing(gameplay_input_routing_config_t config) noexcept;
        [[nodiscard]] gameplay_input_routing_mode_t routing_mode() const noexcept { return _routing_config.mode; }
        [[nodiscard]] size_t player_count() const noexcept { return _player_contexts.size(); }
        [[nodiscard]] const player_input_context_t* player(size_t index) const noexcept;

        void bind(input_action_handle_t action, key_code key, uint8_t required_mods = 0);
        void bind(std::string action, key_code key, uint8_t required_mods = 0);
        void bind_gamepad_button(input_action_handle_t action, gamepad_button_t button);
        void bind_gamepad_button(std::string action, gamepad_button_t button);
        void bind_gamepad_axis(input_action_handle_t action,
                               gamepad_axis_t axis,
                               gamepad_axis_direction_t direction,
                               float threshold = 0.5f);
        void bind_gamepad_axis(std::string action,
                               gamepad_axis_t axis,
                               gamepad_axis_direction_t direction,
                               float threshold = 0.5f);
        void clear() noexcept;
        [[nodiscard]] bool load_bindings_from_file(const io::virtual_file_system_t& vfs, std::string_view virtual_path);

        void update(core::game_context_t& game,
                    const gameplay_input_profile_t& profile,
                    float delta_time) noexcept;
        [[nodiscard]] bool handle_key_event(const events::key_event_t& event,
                                            const gameplay_input_profile_t& profile,
                                            bool log_interact_keys = false) noexcept;
        void on_focus_lost() noexcept;

        [[nodiscard]] bool is_pressed(input_action_id_t action) const noexcept;
        [[nodiscard]] bool was_just_pressed(input_action_id_t action) const noexcept;
        [[nodiscard]] bool is_pressed(size_t player_index, input_action_id_t action) const noexcept;
        [[nodiscard]] bool was_just_pressed(size_t player_index, input_action_id_t action) const noexcept;
        [[nodiscard]] chlm::float2 movement_intent(const core::game_context_t& game,
                                                   const gameplay_input_profile_t& profile) const noexcept;
        [[nodiscard]] chlm::float2 movement_intent(size_t player_index,
                                                   const core::game_context_t& game,
                                                   const gameplay_input_profile_t& profile) const noexcept;
        void apply_player_movement(world::player_controller_t& controller,
                                   const core::game_context_t& game,
                                   const gameplay_input_profile_t& profile) const noexcept;
        void apply_player_movement(size_t player_index,
                                   world::player_controller_t& controller,
                                   const core::game_context_t& game,
                                   const gameplay_input_profile_t& profile) const noexcept;
        [[nodiscard]] bool dispatch_interaction_if_triggered(world::interaction_controller_t& controller,
                                                             core::game_context_t& game,
                                                             const gameplay_input_profile_t& profile) const noexcept;
        [[nodiscard]] bool dispatch_interaction_if_triggered(size_t player_index,
                                                             world::interaction_controller_t& controller,
                                                             core::game_context_t& game,
                                                             const gameplay_input_profile_t& profile) const noexcept;

    private:
        struct runtime_player_context_t
        {
            player_input_context_t descriptor;
            input_action_map_t actions;
            std::unordered_map<input_action_id_t, bool, input_action_id_hash_t> previous_pressed;
            std::unordered_map<input_action_id_t, bool, input_action_id_hash_t> triggered_actions;
        };

        void reset_routing_defaults() noexcept;
        void rebuild_player_contexts() noexcept;
        void sync_context_bindings_if_needed() noexcept;
        [[nodiscard]] runtime_player_context_t* primary_context() noexcept;
        [[nodiscard]] const runtime_player_context_t* primary_context() const noexcept;
        [[nodiscard]] runtime_player_context_t* runtime_context(size_t player_index) noexcept;
        [[nodiscard]] const runtime_player_context_t* runtime_context(size_t player_index) const noexcept;
        void snapshot_action_edges(runtime_player_context_t& context, const gameplay_input_profile_t& profile) noexcept;
        static void track_action_edge(std::unordered_map<input_action_id_t, bool, input_action_id_hash_t>& previous,
                                      std::unordered_map<input_action_id_t, bool, input_action_id_hash_t>& triggered,
                                      const input_action_map_t& actions,
                                      input_action_id_t action) noexcept;

        input_action_map_t _action_bindings;
        gameplay_input_routing_config_t _routing_config{ };
        std::vector<runtime_player_context_t> _player_contexts;
        bool _bindings_dirty{ true };
        bool _move_up_suppressed{ false };
        bool _move_down_suppressed{ false };
        bool _move_left_suppressed{ false };
        bool _move_right_suppressed{ false };
        repeat_state_t _ui_up_repeat;
        repeat_state_t _ui_down_repeat;
        repeat_state_t _ui_left_repeat;
        repeat_state_t _ui_right_repeat;
        bool _ui_accept_gamepad_was_pressed{ false };
        bool _ui_cancel_gamepad_was_pressed{ false };
        float _gamepad_nav_repeat_initial_delay_seconds{ 0.35f };
        float _gamepad_nav_repeat_interval_seconds{ 0.10f };
    };
} // namespace carrot::input
