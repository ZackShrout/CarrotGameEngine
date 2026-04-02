//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Events/Events.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace carrot::input {
    struct action_binding_t
    {
        std::string action;
        key_code key{ key_code::unknown };
        uint8_t required_mods{ 0 };
    };

    class input_action_map_t
    {
    public:
        void bind(std::string action, key_code key, uint8_t required_mods = 0);
        void clear() noexcept;

        void handle_key_event(const events::key_event_t& e) noexcept;

        [[nodiscard]] bool matches(std::string_view action, const events::key_event_t& e) const noexcept;
        [[nodiscard]] bool is_pressed(std::string_view action) const noexcept;

    private:
        [[nodiscard]] bool binding_matches_event(const action_binding_t& binding,
                                                 const events::key_event_t& e) const noexcept;

        std::vector<action_binding_t> _bindings;
        std::unordered_map<std::string, bool, std::hash<std::string>, std::equal_to<>> _pressed_actions;
    };
} // namespace carrot::input
