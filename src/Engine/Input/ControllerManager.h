//
// Created by zshrout on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Input/ControllerState.h"

#include <array>
#include <optional>

namespace carrot::input {
    struct controller_debug_snapshot_t
    {
        uint32_t connected_gamepad_count{ 0 };
        std::optional<uint32_t> active_gamepad_index;
        gamepad_state_t raw_active_gamepad{ };
        gamepad_state_t active_gamepad{ };
        float south_release_pending_seconds{ 0.f };
    };

    class controller_manager_t
    {
    public:
        static constexpr size_t max_gamepad_slots{ 4u };

        void update(float delta_time) noexcept;

        [[nodiscard]] bool has_active_gamepad() const noexcept;
        [[nodiscard]] const gamepad_state_t* active_gamepad() const noexcept;
        [[nodiscard]] gamepad_state_t active_gamepad_snapshot() const noexcept;
        [[nodiscard]] const gamepad_state_t* gamepad(uint32_t slot) const noexcept;
        [[nodiscard]] std::optional<uint32_t> active_gamepad_index() const noexcept { return _active_gamepad_index; }
        [[nodiscard]] controller_debug_snapshot_t debug_snapshot() const noexcept;

    private:
        void update_platform_state() noexcept;
        [[nodiscard]] float platform_button_release_debounce_seconds() const noexcept;
        void log_connection_changes() noexcept;
        void reset_gamepads() noexcept;
        void reset_raw_gamepads() noexcept;
        void stabilize_button_states(const std::array<gamepad_state_t, 4>& previous_gamepads,
                                    float delta_time) noexcept;

        std::array<gamepad_state_t, 4> _raw_gamepads{ };
        std::array<gamepad_state_t, 4> _gamepads{ };
        std::array<bool, 4> _was_connected{ };
        std::array<std::array<float, gamepad_button_count>, 4> _button_release_pending_seconds{ };
        std::optional<uint32_t> _active_gamepad_index;
    };
} // namespace carrot::input
