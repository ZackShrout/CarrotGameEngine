//
// Created by zshrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Input/ControllerState.h"
#include "Events/Events.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::input {
    enum class action_binding_type_t : uint8_t
    {
        key = 0,
        gamepad_button,
        gamepad_axis
    };

    struct action_binding_t
    {
        std::string action;
        action_binding_type_t type{ action_binding_type_t::key };
        key_code key{ key_code::unknown };
        uint8_t required_mods{ 0 };
        gamepad_button_t gamepad_button{ gamepad_button_t::south };
        gamepad_axis_t gamepad_axis{ gamepad_axis_t::left_x };
        gamepad_axis_direction_t gamepad_axis_direction{ gamepad_axis_direction_t::positive };
        float gamepad_axis_threshold{ 0.5f };
        bool active{ false };
    };

    class input_action_map_t
    {
    public:
        void bind(std::string action, key_code key, uint8_t required_mods = 0);
        void bind_gamepad_button(std::string action, gamepad_button_t button);
        void bind_gamepad_axis(std::string action,
                               gamepad_axis_t axis,
                               gamepad_axis_direction_t direction,
                               float threshold = 0.5f);
        void clear() noexcept;
        [[nodiscard]] bool load_bindings_from_memory(const char* data, size_t size);
        [[nodiscard]] bool load_bindings_from_file(const io::virtual_file_system_t& vfs, std::string_view virtual_path);

        void handle_key_event(const events::key_event_t& e) noexcept;
        void update_gamepad_state(const gamepad_state_t* gamepad) noexcept;

        [[nodiscard]] bool matches(std::string_view action, const events::key_event_t& e) const noexcept;
        [[nodiscard]] bool is_pressed(std::string_view action) const noexcept;

    private:
        [[nodiscard]] bool binding_matches_event(const action_binding_t& binding,
                                                 const events::key_event_t& e) const noexcept;
        [[nodiscard]] bool binding_matches_gamepad_state(const action_binding_t& binding,
                                                         const gamepad_state_t& gamepad) const noexcept;
        void refresh_pressed_actions() noexcept;

        std::vector<action_binding_t> _bindings;
        std::unordered_map<std::string, bool, std::hash<std::string>, std::equal_to<>> _pressed_actions;
    };
} // namespace carrot::input
