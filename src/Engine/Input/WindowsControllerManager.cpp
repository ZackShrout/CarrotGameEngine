//
// Created by zshrout on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ControllerManager.h"

#include <Windows.h>
#include <GameInput.h>

namespace carrot::input {
    namespace {
        struct windows_controller_backend_t
        {
            IGameInput* game_input{ nullptr };
            GameInputCallbackToken device_callback_token{ 0 };
            std::mutex mutex;
            std::vector<IGameInputDevice*> devices;
            bool initialized{ false };
        };

        [[nodiscard]] windows_controller_backend_t& backend() noexcept
        {
            static windows_controller_backend_t state;
            return state;
        }

        void release_device(IGameInputDevice*& device) noexcept
        {
            if (!device)
                return;

            device->Release();
            device = nullptr;
        }

        void release_reading(IGameInputReading*& reading) noexcept
        {
            if (!reading)
                return;

            reading->Release();
            reading = nullptr;
        }

        [[nodiscard]] bool contains_device(const std::vector<IGameInputDevice*>& devices,
                                           IGameInputDevice* device) noexcept
        {
            return std::ranges::find(devices, device) != devices.end();
        }

        [[nodiscard]] std::string device_name_for(IGameInputDevice* device) noexcept
        {
            if (!device)
                return "GameInput Gamepad";

            const GameInputDeviceInfo* info{ device->GetDeviceInfo() };
            if (!info || !info->displayName || !info->displayName->data)
                return "GameInput Gamepad";

            return std::string{ info->displayName->data };
        }

        void CALLBACK on_game_input_device_changed([[maybe_unused]] GameInputCallbackToken callback_token,
                                                   void*,
                                                   IGameInputDevice* device,
                                                   [[maybe_unused]] uint64_t timestamp,
                                                   const GameInputDeviceStatus current_status,
                                                   [[maybe_unused]] const GameInputDeviceStatus previous_status)
        {
            windows_controller_backend_t& state{ backend() };
            std::scoped_lock<std::mutex> lock{ state.mutex };

            const bool connected{ (current_status & GameInputDeviceConnected) == GameInputDeviceConnected };
            if (connected)
            {
                if (!contains_device(state.devices, device))
                {
                    device->AddRef();
                    state.devices.push_back(device);
                }
                return;
            }

            const auto it{ std::ranges::find(state.devices, device) };
            if (it != state.devices.end())
            {
                IGameInputDevice* owned_device{ *it };
                state.devices.erase(it);
                release_device(owned_device);
            }
        }

        void ensure_backend_initialized() noexcept
        {
            windows_controller_backend_t& state{ backend() };
            if (state.initialized)
                return;

            if (FAILED(GameInputCreate(&state.game_input)) || !state.game_input)
            {
                LOG_CORE_WARN("GameInputCreate failed; controller input will remain unavailable on Windows");
                state.initialized = true;
                return;
            }

            const HRESULT callback_result{
                state.game_input->RegisterDeviceCallback(nullptr,
                                                         GameInputKindGamepad,
                                                         GameInputDeviceConnected,
                                                         GameInputBlockingEnumeration,
                                                         nullptr,
                                                         on_game_input_device_changed,
                                                         &state.device_callback_token)
            };
            if (FAILED(callback_result))
            {
                LOG_CORE_WARN("GameInput device callback registration failed ({:#x})", callback_result);
            }

            state.initialized = true;
        }
    } // namespace

    void controller_manager_t::update_platform_state() noexcept
    {
        reset_raw_gamepads();
        ensure_backend_initialized();

        windows_controller_backend_t& state{ backend() };
        if (!state.game_input)
            return;

        std::scoped_lock<std::mutex> lock{ state.mutex };

        uint32_t output_index{ 0 };
        for (IGameInputDevice* device : state.devices)
        {
            if (output_index >= _raw_gamepads.size())
                break;

            if ((device->GetDeviceStatus() & GameInputDeviceConnected) != GameInputDeviceConnected)
                continue;

            IGameInputReading* reading{ nullptr };
            const HRESULT reading_result{
                state.game_input->GetCurrentReading(GameInputKindGamepad, device, &reading)
            };
            if (FAILED(reading_result) || !reading)
                continue;

            GameInputGamepadState gamepad_state{ };
            reading->GetGamepadState(&gamepad_state);

            gamepad_state_t& gamepad{ _raw_gamepads[output_index] };
            gamepad.connected = true;
            gamepad.device_name = device_name_for(device);

            const GameInputGamepadButtons buttons{ gamepad_state.buttons };
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::south)] = (buttons & GameInputGamepadA) == GameInputGamepadA;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::east)] = (buttons & GameInputGamepadB) == GameInputGamepadB;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::west)] = (buttons & GameInputGamepadX) == GameInputGamepadX;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::north)] = (buttons & GameInputGamepadY) == GameInputGamepadY;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_up)] = (buttons & GameInputGamepadDPadUp) == GameInputGamepadDPadUp;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_down)] = (buttons & GameInputGamepadDPadDown) == GameInputGamepadDPadDown;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_left)] = (buttons & GameInputGamepadDPadLeft) == GameInputGamepadDPadLeft;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_right)] = (buttons & GameInputGamepadDPadRight) == GameInputGamepadDPadRight;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::left_shoulder)] = (buttons & GameInputGamepadLeftShoulder) == GameInputGamepadLeftShoulder;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::right_shoulder)] = (buttons & GameInputGamepadRightShoulder) == GameInputGamepadRightShoulder;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::left_stick)] = (buttons & GameInputGamepadLeftThumbstick) == GameInputGamepadLeftThumbstick;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::right_stick)] = (buttons & GameInputGamepadRightThumbstick) == GameInputGamepadRightThumbstick;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::back)] = (buttons & GameInputGamepadView) == GameInputGamepadView;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::start)] = (buttons & GameInputGamepadMenu) == GameInputGamepadMenu;

            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_x)] = std::clamp(gamepad_state.leftThumbstickX, -1.0f, 1.0f);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_y)] = -std::clamp(gamepad_state.leftThumbstickY, -1.0f, 1.0f);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_x)] = std::clamp(gamepad_state.rightThumbstickX, -1.0f, 1.0f);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_y)] = -std::clamp(gamepad_state.rightThumbstickY, -1.0f, 1.0f);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_trigger)] = std::clamp(gamepad_state.leftTrigger, 0.0f, 1.0f);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_trigger)] = std::clamp(gamepad_state.rightTrigger, 0.0f, 1.0f);

            release_reading(reading);
            ++output_index;
        }
    }

    float controller_manager_t::platform_button_release_debounce_seconds() const noexcept
    {
        return 0.08f;
    }
} // namespace carrot::input
