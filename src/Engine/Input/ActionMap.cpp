//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ActionMap.h"

namespace carrot::input {
    void input_action_map_t::bind(std::string action, const key_code key, const uint8_t required_mods)
    {
        if (action.empty() || key == key_code::unknown)
            return;

        _bindings.emplace_back(action_binding_t{
            .action = std::move(action),
            .key = key,
            .required_mods = required_mods
        });
    }

    void input_action_map_t::clear() noexcept
    {
        _bindings.clear();
        _pressed_actions.clear();
    }

    void input_action_map_t::handle_key_event(const events::key_event_t& e) noexcept
    {
        if (e._action != events::key_action::press && e._action != events::key_action::release)
            return;

        for (const action_binding_t& binding : _bindings)
        {
            if (!binding_matches_event(binding, e))
                continue;

            _pressed_actions[binding.action] = (e._action == events::key_action::press);
        }
    }

    bool input_action_map_t::matches(const std::string_view action, const events::key_event_t& e) const noexcept
    {
        for (const action_binding_t& binding : _bindings)
        {
            if (binding.action != action)
                continue;

            if (binding_matches_event(binding, e))
                return true;
        }

        return false;
    }

    bool input_action_map_t::is_pressed(const std::string_view action) const noexcept
    {
        const auto it{ _pressed_actions.find(std::string{ action }) };
        return it != _pressed_actions.end() && it->second;
    }

    bool input_action_map_t::binding_matches_event(const action_binding_t& binding,
                                                   const events::key_event_t& e) const noexcept
    {
        if (binding.key != e._key)
            return false;

        return (e._mods & binding.required_mods) == binding.required_mods;
    }
} // namespace carrot::input
