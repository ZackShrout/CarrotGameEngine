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
        [[nodiscard]] gameplay_input_routing_config_t normalize_routing_config(gameplay_input_routing_config_t config) noexcept
        {
            config.player_count = std::clamp<size_t>(config.player_count, 1u, controller_manager_t::max_gamepad_slots);

            if (config.mode == gameplay_input_routing_mode_t::single_player_auto)
                return make_single_player_routing_config();

            return config;
        }

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
                                const input_action_handle_t action_name) noexcept
        {
            if (consume_repeat(pressed,
                               delta_time,
                               state,
                               initial_delay_seconds,
                               repeat_interval_seconds))
            {
                (void)ui_module.dispatch_navigation_action(action_name.authored_id);
            }
        }
    } // namespace

    std::string_view to_string(const gameplay_input_routing_mode_t mode) noexcept
    {
        switch (mode)
        {
            case gameplay_input_routing_mode_t::single_player_auto: return "single_player_auto";
            case gameplay_input_routing_mode_t::local_multiplayer_fixed: return "local_multiplayer_fixed";
        }

        return "unknown";
    }

    gameplay_input_routing_config_t make_single_player_routing_config() noexcept
    {
        gameplay_input_routing_config_t config{ };
        config.mode = gameplay_input_routing_mode_t::single_player_auto;
        config.player_count = 1u;
        config.assignments[0] = player_input_assignment_t{
            .receives_keyboard = true,
            .gamepad_slot = std::nullopt
        };
        return config;
    }

    gameplay_input_routing_config_t make_fixed_local_multiplayer_routing_config(
        const size_t player_count,
        const bool primary_player_receives_keyboard) noexcept
    {
        gameplay_input_routing_config_t config{ };
        config.mode = gameplay_input_routing_mode_t::local_multiplayer_fixed;
        config.player_count = std::clamp<size_t>(player_count, 1u, controller_manager_t::max_gamepad_slots);

        for (size_t index{ 0u }; index < config.player_count; ++index)
        {
            config.assignments[index] = player_input_assignment_t{
                .receives_keyboard = primary_player_receives_keyboard && index == 0u,
                .gamepad_slot = static_cast<uint32_t>(index)
            };
        }

        return config;
    }

    std::string describe_player_input_assignment(const player_input_assignment_t& assignment)
    {
        std::string description{ assignment.receives_keyboard ? "keyboard" : "no keyboard" };
        description += ", gamepad=";
        if (assignment.gamepad_slot.has_value())
            description += std::to_string(*assignment.gamepad_slot);
        else
            description += "none";

        return description;
    }

    std::string describe_player_input_context(const player_input_context_t& context)
    {
        std::string description{ "P" };
        description += std::to_string(context.player_index + 1u);
        description += " (";
        description += describe_player_input_assignment(context.assignment);
        description += ')';
        return description;
    }

    gameplay_input_router_t::gameplay_input_router_t() noexcept
    {
        reset_routing_defaults();
        rebuild_player_contexts();
    }

    void gameplay_input_router_t::configure_routing(gameplay_input_routing_config_t config) noexcept
    {
        _routing_config = normalize_routing_config(config);
        rebuild_player_contexts();
    }

    const player_input_context_t* gameplay_input_router_t::player(const size_t index) const noexcept
    {
        if (const runtime_player_context_t* context{ runtime_context(index) })
            return &context->descriptor;

        return nullptr;
    }

    std::string gameplay_input_router_t::describe_routing() const
    {
        std::string description{ "mode=" };
        description += to_string(_routing_config.mode);
        description += ", players=";
        description += std::to_string(player_count());

        for (size_t index{ 0u }; index < _player_contexts.size(); ++index)
        {
            description += ", ";
            description += describe_player_input_context(_player_contexts[index].descriptor);
        }

        return description;
    }

    void gameplay_input_router_t::bind(const input_action_handle_t action, const key_code key, const uint8_t required_mods)
    {
        _action_bindings.bind(action, key, required_mods);
        _bindings_dirty = true;
    }

    void gameplay_input_router_t::bind(std::string action, const key_code key, const uint8_t required_mods)
    {
        _action_bindings.bind(std::move(action), key, required_mods);
        _bindings_dirty = true;
    }

    void gameplay_input_router_t::bind_gamepad_button(const input_action_handle_t action, const gamepad_button_t button)
    {
        _action_bindings.bind_gamepad_button(action, button);
        _bindings_dirty = true;
    }

    void gameplay_input_router_t::bind_gamepad_button(std::string action, const gamepad_button_t button)
    {
        _action_bindings.bind_gamepad_button(std::move(action), button);
        _bindings_dirty = true;
    }

    void gameplay_input_router_t::bind_gamepad_axis(const input_action_handle_t action,
                                                    const gamepad_axis_t axis,
                                                    const gamepad_axis_direction_t direction,
                                                    const float threshold)
    {
        _action_bindings.bind_gamepad_axis(action, axis, direction, threshold);
        _bindings_dirty = true;
    }

    void gameplay_input_router_t::bind_gamepad_axis(std::string action,
                                                    const gamepad_axis_t axis,
                                                    const gamepad_axis_direction_t direction,
                                                    const float threshold)
    {
        _action_bindings.bind_gamepad_axis(std::move(action), axis, direction, threshold);
        _bindings_dirty = true;
    }

    void gameplay_input_router_t::clear() noexcept
    {
        _action_bindings.clear();
        _bindings_dirty = true;
        on_focus_lost();
    }

    bool gameplay_input_router_t::load_bindings_from_file(const io::virtual_file_system_t& vfs,
                                                          const std::string_view virtual_path)
    {
        const bool loaded{ _action_bindings.load_bindings_from_file(vfs, virtual_path) };
        _bindings_dirty = _bindings_dirty || loaded;
        return loaded;
    }

    void gameplay_input_router_t::update(core::game_context_t& game,
                                         const gameplay_input_profile_t& profile,
                                         const float delta_time) noexcept
    {
        sync_context_bindings_if_needed();

        for (runtime_player_context_t& context : _player_contexts)
        {
            const gamepad_state_t* assigned_gamepad{
                (_routing_config.mode == gameplay_input_routing_mode_t::single_player_auto && context.descriptor.player_index == 0u)
                    ? game.controllers.active_gamepad()
                    : (context.descriptor.assignment.gamepad_slot.has_value()
                    ? game.controllers.gamepad(*context.descriptor.assignment.gamepad_slot)
                    : nullptr)
            };
            context.actions.update_gamepad_state(assigned_gamepad);
        }

        runtime_player_context_t* primary{ primary_context() };
        if (!primary)
            return;

        if (carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() })
        {
            dispatch_if_repeat(*ui_module,
                               primary->actions.is_pressed_by_gamepad(profile.ui_up),
                               delta_time,
                               _ui_up_repeat,
                               _gamepad_nav_repeat_initial_delay_seconds,
                               _gamepad_nav_repeat_interval_seconds,
                               profile.ui_up);
            dispatch_if_repeat(*ui_module,
                               primary->actions.is_pressed_by_gamepad(profile.ui_down),
                               delta_time,
                               _ui_down_repeat,
                               _gamepad_nav_repeat_initial_delay_seconds,
                               _gamepad_nav_repeat_interval_seconds,
                               profile.ui_down);
            dispatch_if_repeat(*ui_module,
                               primary->actions.is_pressed_by_gamepad(profile.ui_left),
                               delta_time,
                               _ui_left_repeat,
                               _gamepad_nav_repeat_initial_delay_seconds,
                               _gamepad_nav_repeat_interval_seconds,
                               profile.ui_left);
            dispatch_if_repeat(*ui_module,
                               primary->actions.is_pressed_by_gamepad(profile.ui_right),
                               delta_time,
                               _ui_right_repeat,
                               _gamepad_nav_repeat_initial_delay_seconds,
                               _gamepad_nav_repeat_interval_seconds,
                               profile.ui_right);

            const bool accept_pressed{ primary->actions.is_pressed_by_gamepad(profile.ui_accept) };
            const bool cancel_pressed{ primary->actions.is_pressed_by_gamepad(profile.ui_cancel) };
            if (accept_pressed && !_ui_accept_gamepad_was_pressed)
                (void)ui_module->dispatch_navigation_action(profile.ui_accept.authored_id);
            if (cancel_pressed && !_ui_cancel_gamepad_was_pressed)
                (void)ui_module->dispatch_navigation_action(profile.ui_cancel.authored_id);
            _ui_accept_gamepad_was_pressed = accept_pressed;
            _ui_cancel_gamepad_was_pressed = cancel_pressed;
        }

        for (runtime_player_context_t& context : _player_contexts)
            snapshot_action_edges(context, profile);
    }

    bool gameplay_input_router_t::handle_key_event(const events::key_event_t& event,
                                                   const gameplay_input_profile_t& profile,
                                                   const bool log_interact_keys) noexcept
    {
        sync_context_bindings_if_needed();
        for (runtime_player_context_t& context : _player_contexts)
        {
            if (context.descriptor.assignment.receives_keyboard)
                context.actions.handle_key_event(event);
        }

        runtime_player_context_t* primary{ primary_context() };
        if (!primary)
            return false;

        bool ui_consumed_event{ false };
        auto handle_ui_key_action = [primary, &event, &ui_consumed_event](const input_action_handle_t action_name) noexcept
        {
            if (!primary->actions.matches(action_name, event))
                return;

            if (event._action == events::key_action::press || event._action == events::key_action::repeat)
            {
                if (ui::ui_module_t* ui_module{ ui::ui_service_t::try_get() })
                    ui_consumed_event = ui_module->dispatch_navigation_action(action_name.authored_id) || ui_consumed_event;
            }
        };

        handle_ui_key_action(profile.ui_up);
        handle_ui_key_action(profile.ui_down);
        handle_ui_key_action(profile.ui_left);
        handle_ui_key_action(profile.ui_right);
        handle_ui_key_action(profile.ui_accept);
        handle_ui_key_action(profile.ui_cancel);

        auto update_move_suppression = [this, &event, ui_consumed_event](const input_action_handle_t move_action,
                                                                         bool& suppressed_flag) noexcept
        {
            const runtime_player_context_t* primary_context_ptr{ primary_context() };
            if (!primary_context_ptr || !primary_context_ptr->actions.matches(move_action, event))
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

        if (event._action == events::key_action::press && primary->actions.matches(profile.interact, event) && log_interact_keys)
        {
            LOG_CORE_INFO("Key pressed: {} ({}) (mods: {})",
                          key_code_to_string(event._key),
                          static_cast<uint32_t>(event._key),
                          modifiers_to_string(event._mods));
        }
        else if (event._action == events::key_action::repeat &&
                 (primary->actions.matches(profile.interact, event) ||
                  event._key == key_code::escape ||
                  event._key == key_code::enter ||
                  event._key == key_code::f11))
        {
            LOG_CORE_INFO("Key held: {} ({})",
                          key_code_to_string(event._key),
                          static_cast<uint32_t>(event._key));
        }
        else if (event._action == events::key_action::release &&
                 (primary->actions.matches(profile.interact, event) ||
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
               primary->actions.matches(profile.toggle_fullscreen, event);
    }

    void gameplay_input_router_t::on_focus_lost() noexcept
    {
        for (runtime_player_context_t& context : _player_contexts)
        {
            context.actions.release_all_keys();
            context.previous_pressed.clear();
            context.triggered_actions.clear();
        }
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
    }

    bool gameplay_input_router_t::is_pressed(const input_action_id_t action) const noexcept
    {
        return is_pressed(0u, action);
    }

    bool gameplay_input_router_t::was_just_pressed(const input_action_id_t action) const noexcept
    {
        return was_just_pressed(0u, action);
    }

    bool gameplay_input_router_t::is_pressed(const size_t player_index, const input_action_id_t action) const noexcept
    {
        const runtime_player_context_t* context{ runtime_context(player_index) };
        return context && context->actions.is_pressed(action);
    }

    bool gameplay_input_router_t::was_just_pressed(const size_t player_index, const input_action_id_t action) const noexcept
    {
        const runtime_player_context_t* context{ runtime_context(player_index) };
        if (!context)
            return false;

        const auto it{ context->triggered_actions.find(action) };
        return it != context->triggered_actions.end() && it->second;
    }

    chlm::float2 gameplay_input_router_t::movement_intent(const core::game_context_t& game,
                                                          const gameplay_input_profile_t& profile) const noexcept
    {
        return movement_intent(0u, game, profile);
    }

    chlm::float2 gameplay_input_router_t::movement_intent(const size_t player_index,
                                                          const core::game_context_t& game,
                                                          const gameplay_input_profile_t& profile) const noexcept
    {
        const runtime_player_context_t* context{ runtime_context(player_index) };
        if (!context)
            return { 0.f, 0.f };

        const gamepad_state_t* gamepad{
            (_routing_config.mode == gameplay_input_routing_mode_t::single_player_auto && player_index == 0u)
                ? game.controllers.active_gamepad()
                : (context->descriptor.assignment.gamepad_slot.has_value()
                ? game.controllers.gamepad(*context->descriptor.assignment.gamepad_slot)
                : nullptr)
        };
        if (gamepad)
        {
            const chlm::float2 left_stick{ gamepad->left_stick() };
            const float length_sq{ (left_stick.x * left_stick.x) + (left_stick.y * left_stick.y) };
            if (length_sq > 0.f)
                return left_stick;
        }

        return digital_move_vector(context->actions,
                                   profile,
                                   player_index == 0u ? _move_up_suppressed : false,
                                   player_index == 0u ? _move_down_suppressed : false,
                                   player_index == 0u ? _move_left_suppressed : false,
                                   player_index == 0u ? _move_right_suppressed : false);
    }

    void gameplay_input_router_t::apply_player_movement(world::player_controller_t& controller,
                                                        const core::game_context_t& game,
                                                        const gameplay_input_profile_t& profile) const noexcept
    {
        controller.set_move_intent(movement_intent(0u, game, profile));
    }

    void gameplay_input_router_t::apply_player_movement(const size_t player_index,
                                                        world::player_controller_t& controller,
                                                        const core::game_context_t& game,
                                                        const gameplay_input_profile_t& profile) const noexcept
    {
        controller.set_move_intent(movement_intent(player_index, game, profile));
    }

    bool gameplay_input_router_t::dispatch_interaction_if_triggered(world::interaction_controller_t& controller,
                                                                    core::game_context_t& game,
                                                                    const gameplay_input_profile_t& profile) const noexcept
    {
        return dispatch_interaction_if_triggered(0u, controller, game, profile);
    }

    bool gameplay_input_router_t::dispatch_interaction_if_triggered(const size_t player_index,
                                                                    world::interaction_controller_t& controller,
                                                                    core::game_context_t& game,
                                                                    const gameplay_input_profile_t& profile) const noexcept
    {
        if (!was_just_pressed(player_index, profile.interact))
            return false;

        const world::interaction_attempt_result_t result{ controller.attempt_interaction(game) };
        switch (result)
        {
            case world::interaction_attempt_result_t::queued:
                return true;
            case world::interaction_attempt_result_t::actor_missing_transform:
                LOG_CORE_WARN("Interaction failed: controlled player world object is missing a transform");
                return false;
            case world::interaction_attempt_result_t::no_actor:
                LOG_CORE_WARN("Interaction failed: no actor is bound to the interaction controller");
                return false;
            case world::interaction_attempt_result_t::no_candidate:
                LOG_CORE_INFO("No interactable in range");
                return false;
            default:
                LOG_CORE_WARN("Interaction failed with unexpected controller result '{}'", world::to_string(result));
                return false;
        }
    }

    void gameplay_input_router_t::reset_routing_defaults() noexcept
    {
        _routing_config = make_single_player_routing_config();
    }

    void gameplay_input_router_t::rebuild_player_contexts() noexcept
    {
        if (_routing_config.player_count == 0u)
            reset_routing_defaults();

        _player_contexts.clear();
        _player_contexts.reserve(_routing_config.player_count);
        for (size_t index{ 0u }; index < _routing_config.player_count; ++index)
        {
            runtime_player_context_t context;
            context.descriptor.player_index = index;
            context.descriptor.assignment = _routing_config.assignments[index];
            _player_contexts.push_back(std::move(context));
        }

        _bindings_dirty = true;
        sync_context_bindings_if_needed();
    }

    void gameplay_input_router_t::sync_context_bindings_if_needed() noexcept
    {
        if (!_bindings_dirty)
            return;

        if (_player_contexts.empty())
        {
            reset_routing_defaults();
            _player_contexts.reserve(_routing_config.player_count);
            for (size_t index{ 0u }; index < _routing_config.player_count; ++index)
            {
                runtime_player_context_t context;
                context.descriptor.player_index = index;
                context.descriptor.assignment = _routing_config.assignments[index];
                _player_contexts.push_back(std::move(context));
            }
        }

        for (runtime_player_context_t& context : _player_contexts)
        {
            context.actions = _action_bindings;
            context.actions.release_all_keys();
            context.previous_pressed.clear();
            context.triggered_actions.clear();
        }

        _bindings_dirty = false;
    }

    gameplay_input_router_t::runtime_player_context_t* gameplay_input_router_t::primary_context() noexcept
    {
        return runtime_context(0u);
    }

    const gameplay_input_router_t::runtime_player_context_t* gameplay_input_router_t::primary_context() const noexcept
    {
        return runtime_context(0u);
    }

    gameplay_input_router_t::runtime_player_context_t* gameplay_input_router_t::runtime_context(const size_t player_index) noexcept
    {
        return player_index < _player_contexts.size() ? &_player_contexts[player_index] : nullptr;
    }

    const gameplay_input_router_t::runtime_player_context_t* gameplay_input_router_t::runtime_context(const size_t player_index) const noexcept
    {
        return player_index < _player_contexts.size() ? &_player_contexts[player_index] : nullptr;
    }

    void gameplay_input_router_t::snapshot_action_edges(runtime_player_context_t& context,
                                                        const gameplay_input_profile_t& profile) noexcept
    {
        context.triggered_actions.clear();

        track_action_edge(context.previous_pressed, context.triggered_actions, context.actions, profile.interact);
        track_action_edge(context.previous_pressed, context.triggered_actions, context.actions, profile.quit);
        track_action_edge(context.previous_pressed, context.triggered_actions, context.actions, profile.toggle_map_collision_debug);
        track_action_edge(context.previous_pressed,
                          context.triggered_actions,
                          context.actions,
                          profile.toggle_object_collision_debug);
        track_action_edge(context.previous_pressed,
                          context.triggered_actions,
                          context.actions,
                          profile.toggle_trigger_volume_debug);
    }

    void gameplay_input_router_t::track_action_edge(
        std::unordered_map<input_action_id_t, bool, input_action_id_hash_t>& previous,
        std::unordered_map<input_action_id_t, bool, input_action_id_hash_t>& triggered,
        const input_action_map_t& actions,
        const input_action_id_t action) noexcept
    {
        if (!action.valid())
            return;

        const bool current{ actions.is_pressed(action) };
        const auto previous_it{ previous.find(action) };
        const bool was_pressed{ previous_it != previous.end() ? previous_it->second : false };
        triggered[action] = current && !was_pressed;
        previous[action] = current;
    }
} // namespace carrot::input
