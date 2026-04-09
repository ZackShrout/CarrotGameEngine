//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "ActionMap.h"

#include <unordered_map>

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
    struct gameplay_input_profile_t
    {
        std::string_view move_up;
        std::string_view move_down;
        std::string_view move_left;
        std::string_view move_right;
        std::string_view interact;
        std::string_view quit;
        std::string_view toggle_fullscreen;
        std::string_view toggle_map_collision_debug;
        std::string_view toggle_object_collision_debug;
        std::string_view ui_up;
        std::string_view ui_down;
        std::string_view ui_left;
        std::string_view ui_right;
        std::string_view ui_accept;
        std::string_view ui_cancel;
    };

    class gameplay_input_router_t
    {
    public:
        struct repeat_state_t
        {
            bool was_pressed{ false };
            float hold_time_seconds{ 0.f };
            float repeat_time_seconds{ 0.f };
        };

        void bind(std::string action, key_code key, uint8_t required_mods = 0);
        void bind_gamepad_button(std::string action, gamepad_button_t button);
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

        [[nodiscard]] bool is_pressed(std::string_view action) const noexcept;
        [[nodiscard]] bool was_just_pressed(std::string_view action) const noexcept;
        [[nodiscard]] chlm::float2 movement_intent(const core::game_context_t& game,
                                                   const gameplay_input_profile_t& profile) const noexcept;
        void apply_player_movement(world::player_controller_t& controller,
                                   const core::game_context_t& game,
                                   const gameplay_input_profile_t& profile) const noexcept;
        [[nodiscard]] bool dispatch_interaction_if_triggered(world::interaction_controller_t& controller,
                                                             core::game_context_t& game,
                                                             const gameplay_input_profile_t& profile) const noexcept;

    private:
        void snapshot_action_edges(const gameplay_input_profile_t& profile) noexcept;
        static void track_action_edge(std::unordered_map<std::string, bool, std::hash<std::string>, std::equal_to<>>& previous,
                                      std::unordered_map<std::string, bool, std::hash<std::string>, std::equal_to<>>& triggered,
                                      const input_action_map_t& actions,
                                      std::string_view action) noexcept;

        input_action_map_t _actions;
        std::unordered_map<std::string, bool, std::hash<std::string>, std::equal_to<>> _previous_pressed;
        std::unordered_map<std::string, bool, std::hash<std::string>, std::equal_to<>> _triggered_actions;
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
