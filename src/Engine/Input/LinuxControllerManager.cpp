//
// Created by Codex on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "ControllerManager.h"

#include <fcntl.h>
#include <libudev.h>
#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <unistd.h>

namespace carrot::input {
    namespace {
        struct linux_gamepad_device_t
        {
            std::string syspath;
            std::string devnode;
            std::string name;
            int fd{ -1 };
            libevdev* evdev{ nullptr };
            std::optional<uint32_t> slot;
        };

        struct linux_controller_backend_t
        {
            udev* udev_context{ nullptr };
            udev_monitor* monitor{ nullptr };
            bool initialized{ false };
            std::vector<linux_gamepad_device_t> devices;
            std::array<std::string, 4> slot_syspaths{ };
        };

        [[nodiscard]] linux_controller_backend_t& backend() noexcept
        {
            static linux_controller_backend_t state;
            return state;
        }

        void release_device(linux_gamepad_device_t& device) noexcept
        {
            if (device.evdev)
            {
                libevdev_free(device.evdev);
                device.evdev = nullptr;
            }

            if (device.fd >= 0)
            {
                close(device.fd);
                device.fd = -1;
            }
        }

        void clear_slot_assignment(linux_controller_backend_t& state, linux_gamepad_device_t& device) noexcept
        {
            if (!device.slot.has_value())
                return;

            if (state.slot_syspaths[*device.slot] == device.syspath)
                state.slot_syspaths[*device.slot].clear();

            device.slot.reset();
        }

        void remove_device(linux_controller_backend_t& state, linux_gamepad_device_t& device) noexcept
        {
            clear_slot_assignment(state, device);
            release_device(device);
        }

        [[nodiscard]] bool is_event_device(udev_device* device) noexcept
        {
            const char* devnode{ udev_device_get_devnode(device) };
            return devnode && std::string_view{ devnode }.starts_with("/dev/input/event");
        }

        [[nodiscard]] bool is_joystick_device(udev_device* device) noexcept
        {
            const char* joystick_property{ udev_device_get_property_value(device, "ID_INPUT_JOYSTICK") };
            return joystick_property && std::string_view{ joystick_property } == "1";
        }

        [[nodiscard]] bool has_gamepad_capabilities(const libevdev* evdev) noexcept
        {
            if (!evdev)
                return false;

            const bool has_primary_button{
                libevdev_has_event_code(evdev, EV_KEY, BTN_GAMEPAD) ||
                libevdev_has_event_code(evdev, EV_KEY, BTN_SOUTH) ||
                libevdev_has_event_code(evdev, EV_KEY, BTN_A)
            };
            const bool has_movement_input{
                libevdev_has_event_code(evdev, EV_ABS, ABS_X) ||
                libevdev_has_event_code(evdev, EV_ABS, ABS_Y) ||
                libevdev_has_event_code(evdev, EV_KEY, BTN_DPAD_UP) ||
                libevdev_has_event_code(evdev, EV_ABS, ABS_HAT0X) ||
                libevdev_has_event_code(evdev, EV_ABS, ABS_HAT0Y)
            };

            return has_primary_button || has_movement_input;
        }

        [[nodiscard]] std::string device_name_for(const libevdev* evdev, const std::string& fallback) noexcept
        {
            if (evdev)
            {
                if (const char* name{ libevdev_get_name(evdev) })
                    return std::string{ name };
            }

            return fallback.empty() ? "Linux Gamepad" : fallback;
        }

        [[nodiscard]] std::optional<uint32_t> slot_for_syspath(const linux_controller_backend_t& state,
                                                               const std::string_view syspath) noexcept
        {
            for (uint32_t index{ 0 }; index < state.slot_syspaths.size(); ++index)
            {
                if (state.slot_syspaths[index] == syspath)
                    return index;
            }

            return std::nullopt;
        }

        void assign_slots(linux_controller_backend_t& state) noexcept
        {
            for (linux_gamepad_device_t& device : state.devices)
            {
                if (device.slot.has_value())
                    continue;

                if (const std::optional<uint32_t> existing_slot{ slot_for_syspath(state, device.syspath) })
                {
                    device.slot = existing_slot;
                    continue;
                }

                for (uint32_t index{ 0 }; index < state.slot_syspaths.size(); ++index)
                {
                    if (!state.slot_syspaths[index].empty())
                        continue;

                    state.slot_syspaths[index] = device.syspath;
                    device.slot = index;
                    break;
                }
            }
        }

        [[nodiscard]] linux_gamepad_device_t* find_device(linux_controller_backend_t& state,
                                                          const std::string_view syspath) noexcept
        {
            const auto it{ std::ranges::find_if(state.devices, [&](const linux_gamepad_device_t& device) {
                return device.syspath == syspath;
            }) };

            return it != state.devices.end() ? &(*it) : nullptr;
        }

        [[nodiscard]] float clamp_unit(const float value) noexcept
        {
            return std::clamp(value, -1.f, 1.f);
        }

        [[nodiscard]] float normalize_centered_axis(const libevdev* evdev, const unsigned int code) noexcept
        {
            if (!evdev || !libevdev_has_event_code(evdev, EV_ABS, code))
                return 0.f;

            const input_absinfo* absinfo{ libevdev_get_abs_info(evdev, code) };
            if (!absinfo)
                return 0.f;

            const float minimum{ static_cast<float>(absinfo->minimum) };
            const float maximum{ static_cast<float>(absinfo->maximum) };
            if (maximum <= minimum)
                return 0.f;

            const float current{ static_cast<float>(libevdev_get_event_value(evdev, EV_ABS, code)) };
            const float center{ (minimum + maximum) * 0.5f };
            const float positive_span{ std::max(1.f, maximum - center) };
            const float negative_span{ std::max(1.f, center - minimum) };
            const float delta{ current - center };
            const float flat{ static_cast<float>(std::max(0, absinfo->flat)) };

            if (std::fabs(delta) <= flat)
                return 0.f;

            if (delta > 0.f)
                return clamp_unit((delta - flat) / std::max(1.f, positive_span - flat));

            return clamp_unit((delta + flat) / std::max(1.f, negative_span - flat));
        }

        [[nodiscard]] float normalize_trigger_axis(const libevdev* evdev, const unsigned int code) noexcept
        {
            if (!evdev || !libevdev_has_event_code(evdev, EV_ABS, code))
                return 0.f;

            const input_absinfo* absinfo{ libevdev_get_abs_info(evdev, code) };
            if (!absinfo)
                return 0.f;

            const float minimum{ static_cast<float>(absinfo->minimum) };
            const float maximum{ static_cast<float>(absinfo->maximum) };
            if (maximum <= minimum)
                return 0.f;

            const float current{ static_cast<float>(libevdev_get_event_value(evdev, EV_ABS, code)) };
            return std::clamp((current - minimum) / (maximum - minimum), 0.f, 1.f);
        }

        [[nodiscard]] bool button_pressed(const libevdev* evdev, const unsigned int code) noexcept
        {
            return evdev &&
                   libevdev_has_event_code(evdev, EV_KEY, code) &&
                   libevdev_get_event_value(evdev, EV_KEY, code) != 0;
        }

        [[nodiscard]] int abs_value(const libevdev* evdev, const unsigned int code) noexcept
        {
            if (!evdev || !libevdev_has_event_code(evdev, EV_ABS, code))
                return 0;

            return libevdev_get_event_value(evdev, EV_ABS, code);
        }

        void populate_button_state(const libevdev* evdev, gamepad_state_t& gamepad) noexcept
        {
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::south)] =
                button_pressed(evdev, BTN_SOUTH) || button_pressed(evdev, BTN_A);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::east)] =
                button_pressed(evdev, BTN_EAST) || button_pressed(evdev, BTN_B);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::west)] =
                button_pressed(evdev, BTN_WEST) || button_pressed(evdev, BTN_X);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::north)] =
                button_pressed(evdev, BTN_NORTH) || button_pressed(evdev, BTN_Y);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::left_shoulder)] = button_pressed(evdev, BTN_TL);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::right_shoulder)] = button_pressed(evdev, BTN_TR);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::left_stick)] = button_pressed(evdev, BTN_THUMBL);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::right_stick)] = button_pressed(evdev, BTN_THUMBR);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::back)] =
                button_pressed(evdev, BTN_SELECT) || button_pressed(evdev, BTN_BACK);
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::start)] =
                button_pressed(evdev, BTN_START);

            const bool dpad_up_button{ button_pressed(evdev, BTN_DPAD_UP) };
            const bool dpad_down_button{ button_pressed(evdev, BTN_DPAD_DOWN) };
            const bool dpad_left_button{ button_pressed(evdev, BTN_DPAD_LEFT) };
            const bool dpad_right_button{ button_pressed(evdev, BTN_DPAD_RIGHT) };
            const int hat_x{ abs_value(evdev, ABS_HAT0X) };
            const int hat_y{ abs_value(evdev, ABS_HAT0Y) };

            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_up)] = dpad_up_button || hat_y < 0;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_down)] = dpad_down_button || hat_y > 0;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_left)] = dpad_left_button || hat_x < 0;
            gamepad.buttons[static_cast<size_t>(gamepad_button_t::dpad_right)] = dpad_right_button || hat_x > 0;
        }

        void populate_axis_state(const libevdev* evdev, gamepad_state_t& gamepad) noexcept
        {
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_x)] = normalize_centered_axis(evdev, ABS_X);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_y)] = normalize_centered_axis(evdev, ABS_Y);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_x)] = normalize_centered_axis(evdev, ABS_RX);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_y)] = normalize_centered_axis(evdev, ABS_RY);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::left_trigger)] = normalize_trigger_axis(evdev, ABS_Z);
            gamepad.axes[static_cast<size_t>(gamepad_axis_t::right_trigger)] = normalize_trigger_axis(evdev, ABS_RZ);
        }

        [[nodiscard]] bool poll_device(linux_gamepad_device_t& device) noexcept
        {
            if (!device.evdev)
                return false;

            input_event event{ };
            for (;;)
            {
                const int result{ libevdev_next_event(device.evdev, LIBEVDEV_READ_FLAG_NORMAL, &event) };
                if (result == LIBEVDEV_READ_STATUS_SUCCESS)
                    continue;

                if (result == LIBEVDEV_READ_STATUS_SYNC)
                {
                    int sync_result{ LIBEVDEV_READ_STATUS_SYNC };
                    while (sync_result == LIBEVDEV_READ_STATUS_SYNC)
                    {
                        sync_result = libevdev_next_event(device.evdev, LIBEVDEV_READ_FLAG_SYNC, &event);
                    }

                    if (sync_result == -ENODEV)
                        return false;

                    continue;
                }

                if (result == -EAGAIN)
                    return true;

                if (result == -ENODEV)
                    return false;

                return true;
            }
        }

        [[nodiscard]] bool open_device_from_udev(linux_controller_backend_t& state, udev_device* udev_device_handle) noexcept
        {
            if (!udev_device_handle || !is_event_device(udev_device_handle) || !is_joystick_device(udev_device_handle))
                return false;

            const char* syspath_cstr{ udev_device_get_syspath(udev_device_handle) };
            const char* devnode_cstr{ udev_device_get_devnode(udev_device_handle) };
            if (!syspath_cstr || !devnode_cstr)
                return false;

            const std::string syspath{ syspath_cstr };
            if (find_device(state, syspath))
                return false;

            const int fd{ open(devnode_cstr, O_RDONLY | O_NONBLOCK | O_CLOEXEC) };
            if (fd < 0)
            {
                LOG_CORE_WARN("Failed to open Linux controller device '{}': {}", devnode_cstr, strerror(errno));
                return false;
            }

            libevdev* evdev{ nullptr };
            const int init_result{ libevdev_new_from_fd(fd, &evdev) };
            if (init_result < 0 || !evdev)
            {
                LOG_CORE_WARN("Failed to initialize libevdev for '{}': {}", devnode_cstr, strerror(-init_result));
                close(fd);
                return false;
            }

            if (!has_gamepad_capabilities(evdev))
            {
                libevdev_free(evdev);
                close(fd);
                return false;
            }

            linux_gamepad_device_t device;
            device.syspath = syspath;
            device.devnode = devnode_cstr;
            device.fd = fd;
            device.evdev = evdev;
            device.name = device_name_for(evdev, udev_device_get_sysname(udev_device_handle)
                                                     ? udev_device_get_sysname(udev_device_handle)
                                                     : "");
            state.devices.push_back(std::move(device));
            assign_slots(state);
            return true;
        }

        void enumerate_devices(linux_controller_backend_t& state) noexcept
        {
            udev_enumerate* enumerate{ udev_enumerate_new(state.udev_context) };
            if (!enumerate)
            {
                LOG_CORE_WARN("Failed to allocate udev enumerator for Linux controller discovery");
                return;
            }

            udev_enumerate_add_match_subsystem(enumerate, "input");
            udev_enumerate_scan_devices(enumerate);

            udev_list_entry* devices{ udev_enumerate_get_list_entry(enumerate) };
            udev_list_entry* entry{ nullptr };
            udev_list_entry_foreach(entry, devices)
            {
                const char* path{ udev_list_entry_get_name(entry) };
                if (!path)
                    continue;

                udev_device* device{ udev_device_new_from_syspath(state.udev_context, path) };
                if (!device)
                    continue;

                static_cast<void>(open_device_from_udev(state, device));
                udev_device_unref(device);
            }

            udev_enumerate_unref(enumerate);
        }

        void process_monitor_events(linux_controller_backend_t& state) noexcept
        {
            if (!state.monitor)
                return;

            for (;;)
            {
                udev_device* device{ udev_monitor_receive_device(state.monitor) };
                if (!device)
                    break;

                const char* action{ udev_device_get_action(device) };
                const char* syspath{ udev_device_get_syspath(device) };
                const std::string_view action_view{ action ? action : "" };

                if (action_view == "remove")
                {
                    if (syspath)
                    {
                        std::erase_if(state.devices, [&](linux_gamepad_device_t& tracked) {
                            const bool matches{ tracked.syspath == syspath };
                            if (matches)
                                remove_device(state, tracked);
                            return matches;
                        });
                    }
                }
                else if (action_view == "add" || action_view == "change" || action_view.empty())
                {
                    static_cast<void>(open_device_from_udev(state, device));
                }

                udev_device_unref(device);
            }

            assign_slots(state);
        }

        void ensure_backend_initialized() noexcept
        {
            linux_controller_backend_t& state{ backend() };
            if (state.initialized)
                return;

            state.udev_context = udev_new();
            if (!state.udev_context)
            {
                LOG_CORE_WARN("Failed to initialize libudev; controller input will remain unavailable on Linux");
                state.initialized = true;
                return;
            }

            state.monitor = udev_monitor_new_from_netlink(state.udev_context, "udev");
            if (state.monitor)
            {
                udev_monitor_filter_add_match_subsystem_devtype(state.monitor, "input", nullptr);
                udev_monitor_enable_receiving(state.monitor);
            }
            else
            {
                LOG_CORE_WARN("Failed to create udev monitor for Linux controller hotplug events");
            }

            enumerate_devices(state);
            assign_slots(state);
            state.initialized = true;
        }
    } // namespace

    void controller_manager_t::update_platform_state() noexcept
    {
        reset_raw_gamepads();
        ensure_backend_initialized();

        linux_controller_backend_t& state{ backend() };
        if (!state.udev_context)
            return;

        process_monitor_events(state);

        std::erase_if(state.devices, [&](linux_gamepad_device_t& device) {
            if (!poll_device(device))
            {
                remove_device(state, device);
                return true;
            }

            return false;
        });

        assign_slots(state);

        for (const linux_gamepad_device_t& device : state.devices)
        {
            if (!device.slot.has_value() || *device.slot >= _raw_gamepads.size() || !device.evdev)
                continue;

            gamepad_state_t& gamepad{ _raw_gamepads[*device.slot] };
            gamepad.connected = true;
            gamepad.device_id = *device.slot;
            gamepad.device_name = device.name;

            populate_button_state(device.evdev, gamepad);
            populate_axis_state(device.evdev, gamepad);
        }
    }

    float controller_manager_t::platform_button_release_debounce_seconds() const noexcept
    {
        return 0.08f;
    }
} // namespace carrot::input
