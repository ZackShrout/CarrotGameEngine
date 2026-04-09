//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameplayInputRouter.h"

#include "Core/GameContext.h"
#include "IO/VirtualFileSystem.h"
#include "UI/UIModule.h"
#include "UI/UIService.h"
#include "World/Controllers/InteractionController.h"
#include "World/Controllers/PlayerController.h"

namespace carrot::input {
    namespace {
        [[nodiscard]] bool consume_repeat(const bool pressed,
                                          const float delta_time,
                                          gameplay_input_router_t::repeat_state_t& state,
                                          const float initial_delay_seconds,
                                          const float repeat_interval_seconds) noexcept
        {
            if (!pressed)
            {
                state.was_pressed = false;
                state.hold_time_seconds = 0.f;
                state.repeat_time_seconds = 0.f;
                return false;
            }

            if (!state.was_pressed)
            {
                state.was_pressed = true;
                state.hold_time_seconds = 0.f;
                state.repeat_time_seconds = 0.f;
                return true;
            }

            state.hold_time_seconds += std::max(0.f, delta_time);
            if (state.hold_time_seconds < initial_delay_seconds)
                return false;

            const float clamped_interval{ std::max(0.0001f, repeat_interval_seconds) };
            state.repeat_time_seconds += std::max(0.f, delta_time);
            if (state.repeat_time_seconds < clamped_interval)
                return false;

            state.repeat_time_seconds = std::fmod(state.repeat_time_seconds, clamped_interval);
            return true;
        }

        [[nodiscard]] chlm::float2 digital_move_vector(const input_action_map_t& actions,
                                                       const gameplay_input_profile_t& profile,
                                                       const bool suppress_up,
                                                       const bool suppress_down,
                                                       const bool suppress_left,
                                                       const bool suppress_right) noexcept
        {
            chlm::float2 movement{ 0.f, 0.f };
            if (actions.is_pressed(profile.move_up) && !suppress_up)
                movement.y -= 1.f;
            if (actions.is_pressed(profile.move_down) && !suppress_down)
                movement.y += 1.f;
            if (actions.is_pressed(profile.move_left) && !suppress_left)
                movement.x -= 1.f;
            if (actions.is_pressed(profile.move_right) && !suppress_right)
                movement.x += 1.f;

            const float length_sq{ (movement.x * movement.x) + (movement.y * movement.y) };
            if (length_sq > 1.f)
            {
                const float length{ std::sqrt(length_sq) };
                movement.x /= length;
                movement.y /= length;
            }

            return movement;
        }

        void dispatch_if_repeat(carrot::ui::ui_module_t& ui_module,
                                const bool pressed,
                                const float delta_time,
                                gameplay_input_router_t::repeat_state_t& state,
                                const float initial_delay_seconds,
                                const float repeat_interval_seconds,
                                const std::string_view action_name) noexcept
        {
            if (consume_repeat(pressed,
                               delta_time,
                               state,
                               initial_delay_seconds,
                               repeat_interval_seconds))
            {
                (void)ui_module.dispatch_navigation_action(action_name);
            }
        }
    } // namespace

    void gameplay_input_router_t::bind(std::string action, const key_code key, const uint8_t required_mods)
    {
        _actions.bind(std::move(action), key, required_mods);
    }

    void gameplay_input_router_t::bind_gamepad_button(std::string action, const gamepad_button_t button)
    {
        _actions.bind_gamepad_button(std::move(action), button);
    }

    void gameplay_input_router_t::bind_gamepad_axis(std::string action,
                                                    const gamepad_axis_t axis,
                                                    const gamepad_axis_direction_t direction,
                                                    const float threshold)
    {
        _actions.bind_gamepad_axis(std::move(action), axis, direction, threshold);
    }

    void gameplay_input_router_t::clear() noexcept
    {
        _actions.clear();
        _previous_pressed.clear();
        _triggered_actions.clear();
        on_focus_lost();
    }

    bool gameplay_input_router_t::load_bindings_from_file(const io::virtual_file_system_t& vfs,
                                                          const std::string_view virtual_path)
    {
        return _actions.load_bindings_from_file(vfs, virtual_path);
    }

    void gameplay_input_router_t::update(core::game_context_t& game,
                                         const gameplay_input_profile_t& profile,
                                         const float delta_time) noexcept
    {
        _actions.update_gamepad_state(game.controllers.active_gamepad());

        if (carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() })
        {
            dispatch_if_repeat(*ui_module,
                               _actions.is_pressed_by_gamepad(profile.ui_up),
                               delta_time,
                               _ui_up_repeat,
                               _gamepad_nav_repeat_initial_delay_seconds,
                               _gamepad_nav_repeat_interval_seconds,
                               profile.ui_up);
            dispatch_if_repeat(*ui_module,
                               _actions.is_pressed_by_gamepad(profile.ui_down),
                               delta_time,
                               _ui_down_repeat,
                               _gamepad_nav_repeat_initial_delay_seconds,
                               _gamepad_nav_repeat_interval_seconds,
                               profile.ui_down);
            dispatch_if_repeat(*ui_module,
                               _actions.is_pressed_by_gamepad(profile.ui_left),
                               delta_time,
                               _ui_left_repeat,
                               _gamepad_nav_repeat_initial_delay_seconds,
                               _gamepad_nav_repeat_interval_seconds,
                               profile.ui_left);
            dispatch_if_repeat(*ui_module,
                               _actions.is_pressed_by_gamepad(profile.ui_right),
                               delta_time,
                               _ui_right_repeat,
                               _gamepad_nav_repeat_initial_delay_seconds,
                               _gamepad_nav_repeat_interval_seconds,
                               profile.ui_right);

            const bool accept_pressed{ _actions.is_pressed_by_gamepad(profile.ui_accept) };
            const bool cancel_pressed{ _actions.is_pressed_by_gamepad(profile.ui_cancel) };
            if (accept_pressed && !_ui_accept_gamepad_was_pressed)
                (void)ui_module->dispatch_navigation_action(profile.ui_accept);
            if (cancel_pressed && !_ui_cancel_gamepad_was_pressed)
                (void)ui_module->dispatch_navigation_action(profile.ui_cancel);
            _ui_accept_gamepad_was_pressed = accept_pressed;
            _ui_cancel_gamepad_was_pressed = cancel_pressed;
        }

        snapshot_action_edges(profile);
    }

    bool gameplay_input_router_t::handle_key_event(const events::key_event_t& event,
                                                   const gameplay_input_profile_t& profile,
                                                   const bool log_interact_keys) noexcept
    {
        _actions.handle_key_event(event);

        bool ui_consumed_event{ false };
        auto handle_ui_key_action = [this, &event, &ui_consumed_event](const std::string_view action_name) noexcept
        {
            if (!_actions.matches(action_name, event))
                return;

            if (event._action == events::key_action::press || event._action == events::key_action::repeat)
            {
                if (ui::ui_module_t* ui_module{ ui::ui_service_t::try_get() })
                    ui_consumed_event = ui_module->dispatch_navigation_action(action_name) || ui_consumed_event;
            }
        };

        handle_ui_key_action(profile.ui_up);
        handle_ui_key_action(profile.ui_down);
        handle_ui_key_action(profile.ui_left);
        handle_ui_key_action(profile.ui_right);
        handle_ui_key_action(profile.ui_accept);
        handle_ui_key_action(profile.ui_cancel);

        auto update_move_suppression = [this, &event, ui_consumed_event](const std::string_view move_action,
                                                                         bool& suppressed_flag) noexcept
        {
            if (!_actions.matches(move_action, event))
                return;

            if (event._action == events::key_action::release)
            {
                suppressed_flag = false;
                return;
            }

            if ((event._action == events::key_action::press || event._action == events::key_action::repeat) &&
                ui_consumed_event)
            {
                suppressed_flag = true;
            }
        };

        update_move_suppression(profile.move_up, _move_up_suppressed);
        update_move_suppression(profile.move_down, _move_down_suppressed);
        update_move_suppression(profile.move_left, _move_left_suppressed);
        update_move_suppression(profile.move_right, _move_right_suppressed);

        if (event._action == events::key_action::press && _actions.matches(profile.interact, event) && log_interact_keys)
        {
            LOG_CORE_INFO("Key pressed: {} ({}) (mods: {})",
                          key_code_to_string(event._key),
                          static_cast<uint32_t>(event._key),
                          modifiers_to_string(event._mods));
        }
        else if (event._action == events::key_action::repeat &&
                 (_actions.matches(profile.interact, event) ||
                  event._key == key_code::escape ||
                  event._key == key_code::enter ||
                  event._key == key_code::f11))
        {
            LOG_CORE_INFO("Key held: {} ({})",
                          key_code_to_string(event._key),
                          static_cast<uint32_t>(event._key));
        }
        else if (event._action == events::key_action::release &&
                 (_actions.matches(profile.interact, event) ||
                  event._key == key_code::escape ||
                  event._key == key_code::enter ||
                  event._key == key_code::f11))
        {
            LOG_CORE_INFO("Key released: {} ({})",
                          key_code_to_string(event._key),
                          static_cast<uint32_t>(event._key));
        }

        return event._action == events::key_action::press &&
               !event._repeat &&
               _actions.matches(profile.toggle_fullscreen, event);
    }

    void gameplay_input_router_t::on_focus_lost() noexcept
    {
        _actions.release_all_keys();
        _move_up_suppressed = false;
        _move_down_suppressed = false;
        _move_left_suppressed = false;
        _move_right_suppressed = false;
        _ui_up_repeat = {};
        _ui_down_repeat = {};
        _ui_left_repeat = {};
        _ui_right_repeat = {};
        _ui_accept_gamepad_was_pressed = false;
        _ui_cancel_gamepad_was_pressed = false;
        _previous_pressed.clear();
        _triggered_actions.clear();
    }

    bool gameplay_input_router_t::is_pressed(const std::string_view action) const noexcept
    {
        return _actions.is_pressed(action);
    }

    bool gameplay_input_router_t::was_just_pressed(const std::string_view action) const noexcept
    {
        const auto it{ _triggered_actions.find(std::string{ action }) };
        return it != _triggered_actions.end() && it->second;
    }

    chlm::float2 gameplay_input_router_t::movement_intent(const core::game_context_t& game,
                                                          const gameplay_input_profile_t& profile) const noexcept
    {
        if (const gamepad_state_t* gamepad{ game.controllers.active_gamepad() })
        {
            const chlm::float2 left_stick{ gamepad->left_stick() };
            const float length_sq{ (left_stick.x * left_stick.x) + (left_stick.y * left_stick.y) };
            if (length_sq > 0.f)
                return left_stick;
        }

        return digital_move_vector(_actions,
                                   profile,
                                   _move_up_suppressed,
                                   _move_down_suppressed,
                                   _move_left_suppressed,
                                   _move_right_suppressed);
    }

    void gameplay_input_router_t::apply_player_movement(world::player_controller_t& controller,
                                                        const core::game_context_t& game,
                                                        const gameplay_input_profile_t& profile) const noexcept
    {
        controller.set_move_intent(movement_intent(game, profile));
    }

    bool gameplay_input_router_t::dispatch_interaction_if_triggered(world::interaction_controller_t& controller,
                                                                    core::game_context_t& game,
                                                                    const gameplay_input_profile_t& profile) const noexcept
    {
        if (!was_just_pressed(profile.interact))
            return false;

        if (!controller.actor() || !controller.actor()->transform)
        {
            LOG_CORE_WARN("Interaction failed: controlled player world object is missing a transform");
            return false;
        }

        if (!controller.try_interact(game))
        {
            LOG_CORE_INFO("No interactable in range");
            return false;
        }

        return true;
    }

    void gameplay_input_router_t::snapshot_action_edges(const gameplay_input_profile_t& profile) noexcept
    {
        _triggered_actions.clear();

        track_action_edge(_previous_pressed, _triggered_actions, _actions, profile.interact);
        track_action_edge(_previous_pressed, _triggered_actions, _actions, profile.quit);
        track_action_edge(_previous_pressed, _triggered_actions, _actions, profile.toggle_map_collision_debug);
        track_action_edge(_previous_pressed, _triggered_actions, _actions, profile.toggle_object_collision_debug);
    }

    void gameplay_input_router_t::track_action_edge(
        std::unordered_map<std::string, bool, std::hash<std::string>, std::equal_to<>>& previous,
        std::unordered_map<std::string, bool, std::hash<std::string>, std::equal_to<>>& triggered,
        const input_action_map_t& actions,
        const std::string_view action) noexcept
    {
        if (action.empty())
            return;

        const bool current{ actions.is_pressed(action) };
        const std::string action_key{ action };
        const auto previous_it{ previous.find(action_key) };
        const bool was_pressed{ previous_it != previous.end() ? previous_it->second : false };
        triggered[action_key] = current && !was_pressed;
        previous[action_key] = current;
    }
} // namespace carrot::input
