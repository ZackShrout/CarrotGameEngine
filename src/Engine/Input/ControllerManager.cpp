//
// Created by Codex on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ControllerManager.h"

namespace carrot::input {
    namespace {
        constexpr float k_button_release_debounce_seconds{ 0.08f };
    } // namespace

    void controller_manager_t::update(const float delta_time) noexcept
    {
        const std::array<gamepad_state_t, 4> previous_gamepads{ _gamepads };
        update_platform_state();
        _gamepads = _raw_gamepads;
        stabilize_button_states(previous_gamepads, delta_time);
        log_connection_changes();

        if (_active_gamepad_index.has_value() &&
            !_gamepads[*_active_gamepad_index].connected)
        {
            LOG_CORE_INFO("Controller {} disconnected and was the active gameplay controller",
                          *_active_gamepad_index);
            _active_gamepad_index.reset();
        }

        if (!_active_gamepad_index.has_value())
        {
            for (uint32_t index{ 0 }; index < _gamepads.size(); ++index)
            {
                if (_gamepads[index].connected)
                {
                    _active_gamepad_index = index;
                    LOG_CORE_INFO("Controller {} is now the active gameplay controller", index);
                    break;
                }
            }
        }
    }

    bool controller_manager_t::has_active_gamepad() const noexcept
    {
        return active_gamepad() != nullptr;
    }

    const gamepad_state_t* controller_manager_t::active_gamepad() const noexcept
    {
        if (!_active_gamepad_index.has_value())
            return nullptr;

        const gamepad_state_t& gamepad{ _gamepads[*_active_gamepad_index] };
        return gamepad.connected ? &gamepad : nullptr;
    }

    gamepad_state_t controller_manager_t::active_gamepad_snapshot() const noexcept
    {
        if (const gamepad_state_t* gamepad{ active_gamepad() })
            return *gamepad;

        return gamepad_state_t{ };
    }

    controller_debug_snapshot_t controller_manager_t::debug_snapshot() const noexcept
    {
        controller_debug_snapshot_t snapshot{ };
        snapshot.active_gamepad_index = _active_gamepad_index;
        snapshot.active_gamepad = active_gamepad_snapshot();

        for (const gamepad_state_t& gamepad : _gamepads)
        {
            if (gamepad.connected)
                ++snapshot.connected_gamepad_count;
        }

        if (snapshot.active_gamepad_index.has_value())
        {
            snapshot.raw_active_gamepad = _raw_gamepads[*snapshot.active_gamepad_index];
            snapshot.south_release_pending_seconds =
                _button_release_pending_seconds[*snapshot.active_gamepad_index][static_cast<size_t>(gamepad_button_t::south)];
        }

        if (!snapshot.active_gamepad.connected)
            snapshot.active_gamepad_index.reset();

        return snapshot;
    }

    void controller_manager_t::log_connection_changes() noexcept
    {
        for (uint32_t index{ 0 }; index < _gamepads.size(); ++index)
        {
            const bool connected{ _gamepads[index].connected };
            if (connected == _was_connected[index])
                continue;

            if (connected)
            {
                LOG_CORE_INFO("Controller {} connected ({})",
                              index,
                              _gamepads[index].device_name.empty() ? "Unknown" : _gamepads[index].device_name);
            }
            else
            {
                LOG_CORE_INFO("Controller {} disconnected", index);
            }

            _was_connected[index] = connected;
        }
    }

    void controller_manager_t::reset_gamepads() noexcept
    {
        for (uint32_t index{ 0 }; index < _gamepads.size(); ++index)
        {
            _gamepads[index] = gamepad_state_t{ };
            _gamepads[index].device_id = index;
        }
    }

    void controller_manager_t::reset_raw_gamepads() noexcept
    {
        for (uint32_t index{ 0 }; index < _raw_gamepads.size(); ++index)
        {
            _raw_gamepads[index] = gamepad_state_t{ };
            _raw_gamepads[index].device_id = index;
        }
    }

    void controller_manager_t::stabilize_button_states(const std::array<gamepad_state_t, 4>& previous_gamepads,
                                                       const float delta_time) noexcept
    {
        for (uint32_t gamepad_index{ 0 }; gamepad_index < _gamepads.size(); ++gamepad_index)
        {
            gamepad_state_t& current{ _gamepads[gamepad_index] };
            const gamepad_state_t& previous{ previous_gamepads[gamepad_index] };

            if (!current.connected)
            {
                _button_release_pending_seconds[gamepad_index].fill(0.f);
                continue;
            }

            for (uint32_t button_index{ 0 }; button_index < gamepad_button_count; ++button_index)
            {
                const bool raw_pressed{ current.buttons[button_index] };
                const bool was_pressed{ previous.connected && previous.buttons[button_index] };

                if (raw_pressed)
                {
                    _button_release_pending_seconds[gamepad_index][button_index] = 0.f;
                    continue;
                }

                if (was_pressed)
                {
                    float& pending_seconds{ _button_release_pending_seconds[gamepad_index][button_index] };
                    pending_seconds += std::max(0.f, delta_time);

                    if (pending_seconds < k_button_release_debounce_seconds)
                    {
                        current.buttons[button_index] = true;
                        continue;
                    }
                }

                _button_release_pending_seconds[gamepad_index][button_index] = 0.f;
            }
        }
    }
} // namespace carrot::input
