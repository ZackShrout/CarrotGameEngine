//
// Created by zshrout on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <chlm/CarrotHLM.h>

namespace carrot::input {
    enum class gamepad_button_t : uint8_t
    {
        south = 0,
        east,
        west,
        north,
        dpad_up,
        dpad_down,
        dpad_left,
        dpad_right,
        left_shoulder,
        right_shoulder,
        left_stick,
        right_stick,
        back,
        start,
        count
    };

    enum class gamepad_axis_t : uint8_t
    {
        left_x = 0,
        left_y,
        right_x,
        right_y,
        left_trigger,
        right_trigger,
        count
    };

    enum class gamepad_axis_direction_t : uint8_t
    {
        negative = 0,
        positive
    };

    constexpr size_t gamepad_button_count{ static_cast<size_t>(gamepad_button_t::count) };
    constexpr size_t gamepad_axis_count{ static_cast<size_t>(gamepad_axis_t::count) };

    struct gamepad_state_t
    {
        bool connected{ false };
        uint32_t device_id{ 0 };
        std::string device_name;
        std::array<bool, gamepad_button_count> buttons{ };
        std::array<float, gamepad_axis_count> axes{ };

        [[nodiscard]] bool is_pressed(const gamepad_button_t button) const noexcept
        {
            return buttons[static_cast<size_t>(button)];
        }

        [[nodiscard]] float axis_value(const gamepad_axis_t axis) const noexcept
        {
            return axes[static_cast<size_t>(axis)];
        }

        [[nodiscard]] chlm::float2 left_stick() const noexcept
        {
            return {
                axis_value(gamepad_axis_t::left_x),
                axis_value(gamepad_axis_t::left_y)
            };
        }

        [[nodiscard]] chlm::float2 right_stick() const noexcept
        {
            return {
                axis_value(gamepad_axis_t::right_x),
                axis_value(gamepad_axis_t::right_y)
            };
        }
    };

    constexpr const char* gamepad_button_to_string(const gamepad_button_t button) noexcept
    {
        switch (button)
        {
            case gamepad_button_t::south: return "South";
            case gamepad_button_t::east: return "East";
            case gamepad_button_t::west: return "West";
            case gamepad_button_t::north: return "North";
            case gamepad_button_t::dpad_up: return "DPadUp";
            case gamepad_button_t::dpad_down: return "DPadDown";
            case gamepad_button_t::dpad_left: return "DPadLeft";
            case gamepad_button_t::dpad_right: return "DPadRight";
            case gamepad_button_t::left_shoulder: return "LeftShoulder";
            case gamepad_button_t::right_shoulder: return "RightShoulder";
            case gamepad_button_t::left_stick: return "LeftStick";
            case gamepad_button_t::right_stick: return "RightStick";
            case gamepad_button_t::back: return "Back";
            case gamepad_button_t::start: return "Start";
            case gamepad_button_t::count: return "Unknown";
        }

        return "Unknown";
    }

    constexpr const char* gamepad_axis_to_string(const gamepad_axis_t axis) noexcept
    {
        switch (axis)
        {
            case gamepad_axis_t::left_x: return "LeftX";
            case gamepad_axis_t::left_y: return "LeftY";
            case gamepad_axis_t::right_x: return "RightX";
            case gamepad_axis_t::right_y: return "RightY";
            case gamepad_axis_t::left_trigger: return "LeftTrigger";
            case gamepad_axis_t::right_trigger: return "RightTrigger";
            case gamepad_axis_t::count: return "Unknown";
        }

        return "Unknown";
    }

    constexpr const char* gamepad_axis_direction_to_string(const gamepad_axis_direction_t direction) noexcept
    {
        switch (direction)
        {
            case gamepad_axis_direction_t::negative: return "Negative";
            case gamepad_axis_direction_t::positive: return "Positive";
        }

        return "Unknown";
    }
} // namespace carrot::input
