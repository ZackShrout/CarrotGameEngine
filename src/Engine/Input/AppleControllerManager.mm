//
// Created by Codex on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ControllerManager.h"

#include <GameController/GameController.h>

namespace carrot::input {
    namespace {
        struct apple_controller_backend_t
        {
            bool initialized{ false };
            std::array<GCController*, 4> slot_controllers{ };
        };

        [[nodiscard]] apple_controller_backend_t& backend() noexcept
        {
            static apple_controller_backend_t state;
            return state;
        }

        void release_controller(GCController*& controller) noexcept
        {
            if (!controller)
                return;

            [controller release];
            controller = nil;
        }

        [[nodiscard]] bool is_supported_controller(GCController* controller) noexcept
        {
            if (!controller)
                return false;

            return controller.extendedGamepad != nil || controller.microGamepad != nil;
        }

        [[nodiscard]] bool array_contains_controller(NSArray<GCController*>* controllers,
                                                     GCController* controller) noexcept
        {
            if (!controllers || !controller)
                return false;

            for (GCController* candidate in controllers)
            {
                if (candidate == controller)
                    return true;
            }

            return false;
        }

        [[nodiscard]] bool is_assigned_to_slot(const apple_controller_backend_t& state,
                                               GCController* controller) noexcept
        {
            if (!controller)
                return false;

            return std::ranges::find(state.slot_controllers, controller) != state.slot_controllers.end();
        }

        [[nodiscard]] const char* controller_name(GCController* controller) noexcept
        {
            if (!controller || !controller.vendorName)
                return "Apple Game Controller";

            const char* utf8_name{ [controller.vendorName UTF8String] };
            return utf8_name ? utf8_name : "Apple Game Controller";
        }

        [[nodiscard]] float clamp_unit(const float value) noexcept
        {
            return std::clamp(value, -1.f, 1.f);
        }

        [[nodiscard]] float clamp_trigger(const float value) noexcept
        {
            return std::clamp(value, 0.f, 1.f);
        }

        void ensure_backend_initialized() noexcept
        {
            apple_controller_backend_t& state{ backend() };
            if (state.initialized)
                return;

            [GCController setShouldMonitorBackgroundEvents:NO];
            state.initialized = true;
        }

        void refresh_slot_assignments(apple_controller_backend_t& state, NSArray<GCController*>* controllers) noexcept
        {
            for (GCController*& slot_controller : state.slot_controllers)
            {
                if (!slot_controller)
                    continue;

                if (!array_contains_controller(controllers, slot_controller))
                    release_controller(slot_controller);
            }

            for (GCController* controller in controllers)
            {
                if (!is_supported_controller(controller) || is_assigned_to_slot(state, controller))
                    continue;

                for (GCController*& slot_controller : state.slot_controllers)
                {
                    if (slot_controller)
                        continue;

                    slot_controller = [controller retain];
                    break;
                }
            }
        }

        void populate_from_extended_profile(gamepad_state_t& gamepad,
                                            GCExtendedGamepad* profile) noexcept
        {
            if (!profile)
                return;

            gamepad.buttons[static_cast<size_t>(gamepad_button_t::south)] = profile.buttonA.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::east)] = profile.buttonB.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::west)] = profile.buttonX.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::north)] = profile.buttonY.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_up)] = profile.dpad.up.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_down)] = profile.dpad.down.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_left)] = profile.dpad.left.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_right)] = profile.dpad.right.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::left_shoulder)] = profile.leftShoulder.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::right_shoulder)] = profile.rightShoulder.isPressed;

            if (@available(macOS 11.0, *))
            {
                gamepad.buttons[static_cast<size_t>(gamepad_button_t::left_stick)] = profile.leftThumbstickButton.isPressed;
                gamepad.buttons[static_cast<size_t>(gamepad_button_t::right_stick)] = profile.rightThumbstickButton.isPressed;
                gamepad.buttons[static_cast<size_t>(gamepad_button_t::back)] = profile.buttonOptions.isPressed;
                gamepad.buttons[static_cast<size_t>(gamepad_button_t::start)] = profile.buttonMenu.isPressed;
            }

            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_x)] = clamp_unit(profile.leftThumbstick.xAxis.value);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_y)] = -clamp_unit(profile.leftThumbstick.yAxis.value);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_x)] = clamp_unit(profile.rightThumbstick.xAxis.value);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_y)] = -clamp_unit(profile.rightThumbstick.yAxis.value);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_trigger)] = clamp_trigger(profile.leftTrigger.value);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_trigger)] = clamp_trigger(profile.rightTrigger.value);
        }

        void populate_from_micro_profile(gamepad_state_t& gamepad,
                                         GCMicroGamepad* profile) noexcept
        {
            if (!profile)
                return;

            gamepad.buttons[static_cast<size_t>(gamepad_button_t::south)] = profile.buttonA.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::west)] = profile.buttonX.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_up)] = profile.dpad.up.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_down)] = profile.dpad.down.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_left)] = profile.dpad.left.isPressed;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_right)] = profile.dpad.right.isPressed;

            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_x)] = clamp_unit(profile.dpad.xAxis.value);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_y)] = -clamp_unit(profile.dpad.yAxis.value);
        }
    } // namespace

    void controller_manager_t::update_platform_state() noexcept
    {
        reset_raw_gamepads();

        ensure_backend_initialized();
        apple_controller_backend_t& state{ backend() };
        NSArray<GCController*>* controllers{ [GCController controllers] };
        refresh_slot_assignments(state, controllers);

        for (uint32_t slot{ 0 }; slot < _raw_gamepads.size(); ++slot)
        {
            GCController* controller{ state.slot_controllers[slot] };
            if (!controller || !is_supported_controller(controller))
                continue;

            gamepad_state_t& gamepad{ _raw_gamepads[slot] };
            gamepad.connected = true;
            gamepad.device_id = slot;
            gamepad.device_name = controller_name(controller);

            if (GCExtendedGamepad* extended{ controller.extendedGamepad })
            {
                populate_from_extended_profile(gamepad, extended);
                continue;
            }

            if (GCMicroGamepad* micro{ controller.microGamepad })
                populate_from_micro_profile(gamepad, micro);
        }
    }

    float controller_manager_t::platform_button_release_debounce_seconds() const noexcept
    {
        return 0.f;
    }
} // namespace carrot::input
