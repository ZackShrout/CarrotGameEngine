//
// Created by zshrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Input/ControllerState.h"
#include "Input/InputActionId.h"
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
        input_action_id_t action_id{ };
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
        [[nodiscard]] const std::vector<action_binding_t>& bindings() const noexcept { return _bindings; }
        [[nodiscard]] std::vector<action_binding_t> bindings_for_action(input_action_id_t action) const;
        [[nodiscard]] std::vector<action_binding_t> bindings_for_action(std::string_view action) const;

        void add_binding(action_binding_t binding);
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
        void set_bindings_for_action(input_action_handle_t action, std::vector<action_binding_t> bindings);
        void clear_bindings_for_action(input_action_id_t action) noexcept;
        void clear_bindings_for_action(std::string_view action) noexcept;
        void clear() noexcept;
        [[nodiscard]] std::string serialize_bindings_to_json() const;
        [[nodiscard]] bool load_bindings_from_memory(const char* data, size_t size);
        [[nodiscard]] bool load_bindings_from_file(const io::virtual_file_system_t& vfs, std::string_view virtual_path);

        void handle_key_event(const events::key_event_t& e) noexcept;
        void update_gamepad_state(const gamepad_state_t* gamepad) noexcept;
        void release_all_keys() noexcept;

        [[nodiscard]] bool matches(input_action_id_t action, const events::key_event_t& e) const noexcept;
        [[nodiscard]] bool matches(std::string_view action, const events::key_event_t& e) const noexcept;
        [[nodiscard]] bool is_pressed(input_action_id_t action) const noexcept;
        [[nodiscard]] bool is_pressed(std::string_view action) const noexcept;
        [[nodiscard]] bool is_pressed_by_gamepad(input_action_id_t action) const noexcept;
        [[nodiscard]] bool is_pressed_by_gamepad(std::string_view action) const noexcept;

    private:
        static void finalize_binding(action_binding_t& binding) noexcept;
        [[nodiscard]] bool binding_matches_event(const action_binding_t& binding,
                                                 const events::key_event_t& e) const noexcept;
        [[nodiscard]] bool binding_matches_gamepad_state(const action_binding_t& binding,
                                                         const gamepad_state_t& gamepad) const noexcept;
        void refresh_pressed_actions() noexcept;

        std::vector<action_binding_t> _bindings;
        std::unordered_map<input_action_id_t, bool, input_action_id_hash_t> _pressed_actions;
        std::unordered_map<input_action_id_t, bool, input_action_id_hash_t> _pressed_actions_gamepad;
    };
} // namespace carrot::input
