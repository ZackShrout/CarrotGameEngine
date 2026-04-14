//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "ActionMap.h"

#include <optional>

namespace carrot::input {
    enum class input_rebind_session_state_t : std::uint8_t
    {
        idle = 0,
        listening,
        captured,
        cancelled,
    };

    struct input_rebind_request_t
    {
        input_action_handle_t action{ };
        bool allow_keys{ true };
        bool allow_gamepad_buttons{ true };
        bool allow_gamepad_axes{ true };
        bool allow_modifier_keys{ false };
        bool replace_existing_bindings{ true };
        float gamepad_axis_capture_threshold{ 0.5f };
    };

    struct input_rebind_capture_t
    {
        input_action_handle_t action{ };
        action_binding_t binding{ };
        bool replace_existing_bindings{ true };
    };

    class input_rebind_session_t
    {
    public:
        void begin(input_rebind_request_t request) noexcept;
        void cancel() noexcept;
        void reset() noexcept;

        [[nodiscard]] input_rebind_session_state_t state() const noexcept { return _state; }
        [[nodiscard]] bool listening() const noexcept { return _state == input_rebind_session_state_t::listening; }
        [[nodiscard]] bool has_capture() const noexcept { return _capture.has_value(); }
        [[nodiscard]] const std::optional<input_rebind_capture_t>& capture() const noexcept { return _capture; }
        [[nodiscard]] std::optional<input_rebind_capture_t> consume_capture() noexcept;
        [[nodiscard]] const std::optional<input_rebind_request_t>& request() const noexcept { return _request; }

        [[nodiscard]] bool handle_key_event(const events::key_event_t& event) noexcept;
        [[nodiscard]] bool update_gamepad_state(const gamepad_state_t* gamepad) noexcept;
        [[nodiscard]] bool apply_capture(input_action_map_t& action_map) const;

    private:
        void store_capture(action_binding_t binding) noexcept;
        [[nodiscard]] static bool is_modifier_key(key_code key) noexcept;

        input_rebind_session_state_t _state{ input_rebind_session_state_t::idle };
        std::optional<input_rebind_request_t> _request;
        std::optional<input_rebind_capture_t> _capture;
        gamepad_state_t _previous_gamepad{ };
        bool _has_previous_gamepad{ false };
    };
} // namespace carrot::input
