//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

namespace sandbox {
    inline constexpr std::string_view k_action_move_up{ "move_up" };
    inline constexpr std::string_view k_action_move_down{ "move_down" };
    inline constexpr std::string_view k_action_move_left{ "move_left" };
    inline constexpr std::string_view k_action_move_right{ "move_right" };
    inline constexpr std::string_view k_action_interact{ "interact" };
    inline constexpr std::string_view k_action_quit{ "quit" };
    inline constexpr std::string_view k_action_toggle_fullscreen{ "toggle_fullscreen" };
    inline constexpr std::string_view k_action_toggle_map_collision_debug{ "toggle_map_collision_debug" };
    inline constexpr std::string_view k_action_toggle_object_collision_debug{ "toggle_object_collision_debug" };
    inline constexpr std::string_view k_action_ui_up{ "ui_up" };
    inline constexpr std::string_view k_action_ui_down{ "ui_down" };
    inline constexpr std::string_view k_action_ui_left{ "ui_left" };
    inline constexpr std::string_view k_action_ui_right{ "ui_right" };
    inline constexpr std::string_view k_action_ui_accept{ "ui_accept" };
    inline constexpr std::string_view k_action_ui_cancel{ "ui_cancel" };
    inline constexpr std::string_view k_input_bindings_config_path{ "game://config/input_actions.json" };

    inline constexpr carrot::input::gameplay_input_profile_t k_input_profile{
        .move_up = k_action_move_up,
        .move_down = k_action_move_down,
        .move_left = k_action_move_left,
        .move_right = k_action_move_right,
        .interact = k_action_interact,
        .quit = k_action_quit,
        .toggle_fullscreen = k_action_toggle_fullscreen,
        .toggle_map_collision_debug = k_action_toggle_map_collision_debug,
        .toggle_object_collision_debug = k_action_toggle_object_collision_debug,
        .ui_up = k_action_ui_up,
        .ui_down = k_action_ui_down,
        .ui_left = k_action_ui_left,
        .ui_right = k_action_ui_right,
        .ui_accept = k_action_ui_accept,
        .ui_cancel = k_action_ui_cancel,
    };
} // namespace sandbox
