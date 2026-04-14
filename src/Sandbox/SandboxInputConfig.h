//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

#include <array>

namespace sandbox {
    inline constexpr std::string_view k_input_bindings_config_path{ "game://config/input_actions.json" };
    inline constexpr std::string_view k_user_input_bindings_config_path{ "save://config/input_actions.json" };

    inline constexpr carrot::input::gameplay_input_profile_t k_input_actions{
        .move_up = carrot::input::make_input_action("move_up"),
        .move_down = carrot::input::make_input_action("move_down"),
        .move_left = carrot::input::make_input_action("move_left"),
        .move_right = carrot::input::make_input_action("move_right"),
        .interact = carrot::input::make_input_action("interact"),
        .quit = carrot::input::make_input_action("quit"),
        .toggle_fullscreen = carrot::input::make_input_action("toggle_fullscreen"),
        .toggle_map_collision_debug = carrot::input::make_input_action("toggle_map_collision_debug"),
        .toggle_object_collision_debug = carrot::input::make_input_action("toggle_object_collision_debug"),
        .ui_up = carrot::input::make_input_action("ui_up"),
        .ui_down = carrot::input::make_input_action("ui_down"),
        .ui_left = carrot::input::make_input_action("ui_left"),
        .ui_right = carrot::input::make_input_action("ui_right"),
        .ui_accept = carrot::input::make_input_action("ui_accept"),
        .ui_cancel = carrot::input::make_input_action("ui_cancel"),
    };

    inline constexpr std::array<carrot::input::input_action_definition_t, 15> k_input_action_definitions{ {
        { .action = k_input_actions.move_up, .display_name = "Move Up", .category = "Gameplay" },
        { .action = k_input_actions.move_down, .display_name = "Move Down", .category = "Gameplay" },
        { .action = k_input_actions.move_left, .display_name = "Move Left", .category = "Gameplay" },
        { .action = k_input_actions.move_right, .display_name = "Move Right", .category = "Gameplay" },
        { .action = k_input_actions.interact, .display_name = "Interact", .category = "Gameplay" },
        { .action = k_input_actions.quit, .display_name = "Quit", .category = "System" },
        { .action = k_input_actions.toggle_fullscreen, .display_name = "Toggle Fullscreen", .category = "System" },
        { .action = k_input_actions.toggle_map_collision_debug, .display_name = "Toggle Map Collision Debug", .category = "Debug" },
        { .action = k_input_actions.toggle_object_collision_debug, .display_name = "Toggle Object Collider Debug", .category = "Debug" },
        { .action = k_input_actions.ui_up, .display_name = "UI Up", .category = "UI" },
        { .action = k_input_actions.ui_down, .display_name = "UI Down", .category = "UI" },
        { .action = k_input_actions.ui_left, .display_name = "UI Left", .category = "UI" },
        { .action = k_input_actions.ui_right, .display_name = "UI Right", .category = "UI" },
        { .action = k_input_actions.ui_accept, .display_name = "UI Accept", .category = "UI" },
        { .action = k_input_actions.ui_cancel, .display_name = "UI Cancel", .category = "UI" },
    } };

    inline constexpr carrot::input::input_action_catalog_view_t k_input_action_catalog{ k_input_action_definitions };

    inline constexpr const carrot::input::gameplay_input_profile_t& k_input_profile{ k_input_actions };
} // namespace sandbox
