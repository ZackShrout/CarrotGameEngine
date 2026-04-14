//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "InputRebindSession.h"

namespace carrot::input {
    void input_rebind_session_t::begin(input_rebind_request_t request) noexcept
    {
        if (!request.action)
            return;

        request.gamepad_axis_capture_threshold = std::clamp(request.gamepad_axis_capture_threshold, 0.05f, 1.0f);
        _request = request;
        _capture.reset();
        _previous_gamepad = { };
        _has_previous_gamepad = false;
        _state = input_rebind_session_state_t::listening;
    }

    void input_rebind_session_t::cancel() noexcept
    {
        _capture.reset();
        _state = input_rebind_session_state_t::cancelled;
    }

    void input_rebind_session_t::reset() noexcept
    {
        _request.reset();
        _capture.reset();
        _previous_gamepad = { };
        _has_previous_gamepad = false;
        _state = input_rebind_session_state_t::idle;
    }

    std::optional<input_rebind_capture_t> input_rebind_session_t::consume_capture() noexcept
    {
        std::optional<input_rebind_capture_t> result{ std::move(_capture) };
        _capture.reset();
        if (_state == input_rebind_session_state_t::captured)
            _state = input_rebind_session_state_t::idle;
        _request.reset();
        _previous_gamepad = { };
        _has_previous_gamepad = false;
        return result;
    }

    bool input_rebind_session_t::handle_key_event(const events::key_event_t& event) noexcept
    {
        if (!listening() || !_request || !_request->allow_keys)
            return false;

        if (event._action != events::key_action::press || event._repeat || event._key == key_code::unknown)
            return false;

        if (!_request->allow_modifier_keys && is_modifier_key(event._key))
            return false;

        action_binding_t binding{
            .action = std::string{ _request->action.authored_id },
            .action_id = _request->action.id,
            .type = action_binding_type_t::key,
            .key = event._key,
            .required_mods = static_cast<std::uint8_t>(is_modifier_key(event._key) ? 0u : event._mods),
        };

        store_capture(std::move(binding));
        return true;
    }

    bool input_rebind_session_t::update_gamepad_state(const gamepad_state_t* gamepad) noexcept
    {
        if (!listening() || !_request)
            return false;

        if (!gamepad || !gamepad->connected)
        {
            _previous_gamepad = { };
            _has_previous_gamepad = false;
            return false;
        }

        const gamepad_state_t previous{ _has_previous_gamepad ? _previous_gamepad : gamepad_state_t{} };
        _previous_gamepad = *gamepad;
        _has_previous_gamepad = true;

        if (_request->allow_gamepad_buttons)
        {
            for (std::uint8_t i{ 0u }; i < static_cast<std::uint8_t>(gamepad_button_t::count); ++i)
            {
                const gamepad_button_t button{ static_cast<gamepad_button_t>(i) };
                if (gamepad->is_pressed(button) && !previous.is_pressed(button))
                {
                    action_binding_t binding{
                        .action = std::string{ _request->action.authored_id },
                        .action_id = _request->action.id,
                        .type = action_binding_type_t::gamepad_button,
                        .gamepad_button = button,
                    };
                    store_capture(std::move(binding));
                    return true;
                }
            }
        }

        if (_request->allow_gamepad_axes)
        {
            for (std::uint8_t i{ 0u }; i < static_cast<std::uint8_t>(gamepad_axis_t::count); ++i)
            {
                const gamepad_axis_t axis{ static_cast<gamepad_axis_t>(i) };
                const float current_value{ gamepad->axis_value(axis) };
                const float previous_value{ previous.axis_value(axis) };
                const float threshold{ _request->gamepad_axis_capture_threshold };

                if (std::fabs(current_value) < threshold || std::fabs(previous_value) >= threshold)
                    continue;

                action_binding_t binding{
                    .action = std::string{ _request->action.authored_id },
                    .action_id = _request->action.id,
                    .type = action_binding_type_t::gamepad_axis,
                    .gamepad_axis = axis,
                    .gamepad_axis_direction = current_value < 0.f ? gamepad_axis_direction_t::negative
                                                                 : gamepad_axis_direction_t::positive,
                    .gamepad_axis_threshold = threshold,
                };
                store_capture(std::move(binding));
                return true;
            }
        }

        return false;
    }

    bool input_rebind_session_t::apply_capture(input_action_map_t& action_map) const
    {
        if (!_capture)
            return false;

        if (_capture->replace_existing_bindings)
            action_map.set_bindings_for_action(_capture->action, { _capture->binding });
        else
            action_map.add_binding(_capture->binding);

        return true;
    }

    void input_rebind_session_t::store_capture(action_binding_t binding) noexcept
    {
        if (!_request)
            return;

        _capture = input_rebind_capture_t{
            .action = _request->action,
            .binding = std::move(binding),
            .replace_existing_bindings = _request->replace_existing_bindings,
        };
        _state = input_rebind_session_state_t::captured;
    }

    bool input_rebind_session_t::is_modifier_key(const key_code key) noexcept
    {
        switch (key)
        {
            case key_code::left_shift:
            case key_code::right_shift:
            case key_code::left_control:
            case key_code::right_control:
            case key_code::left_alt:
            case key_code::right_alt:
            case key_code::left_super:
            case key_code::right_super:
                return true;
            default:
                return false;
        }
    }
} // namespace carrot::input
