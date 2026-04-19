//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SandboxGame.h"

#include "GameplayState.h"
#include "SandboxInputConfig.h"

namespace sandbox {
    namespace {
        void ensure_action_has_default_binding(carrot::input::input_binding_store_t& store,
                                               const carrot::input::input_action_handle_t action,
                                               const char* label)
        {
            const auto summary{ store.describe_action(action) };
            if (!summary.has_value() || !summary->active_bindings.empty())
                return;

            if (store.restore_action_defaults(action, false))
                LOG_CORE_INFO("Restored missing default binding for {}", label);
        }

        [[nodiscard]] std::string describe_binding(const carrot::input::action_binding_t& binding)
        {
            using namespace carrot::input;

            switch (binding.type)
            {
                case action_binding_type_t::key:
                {
                    std::string description;
                    if (binding.required_mods != 0)
                    {
                        description = modifiers_to_string(binding.required_mods);
                        description += '+';
                    }
                    description += key_code_to_string(binding.key);
                    return description;
                }
                case action_binding_type_t::gamepad_button:
                    return std::string{ gamepad_button_to_string(binding.gamepad_button) };
                case action_binding_type_t::gamepad_axis:
                {
                    char threshold_buffer[16]{ };
                    std::snprintf(threshold_buffer, sizeof(threshold_buffer), "%.2f", binding.gamepad_axis_threshold);
                    std::string description{ gamepad_axis_to_string(binding.gamepad_axis) };
                    description += ' ';
                    description += gamepad_axis_direction_to_string(binding.gamepad_axis_direction);
                    description += " >= ";
                    description += threshold_buffer;
                    return description;
                }
            }

            return "Unknown";
        }

        [[nodiscard]] const char* ui_feedback_event_to_string(const carrot::ui::ui_feedback_event_t event) noexcept
        {
            switch (event)
            {
                case carrot::ui::ui_feedback_event_t::focus_move: return "focus_move";
                case carrot::ui::ui_feedback_event_t::accept: return "accept";
                case carrot::ui::ui_feedback_event_t::cancel: return "cancel";
                default: return "unknown";
            }
        }
    } // namespace

    sandbox_game_t::sandbox_game_t(carrot::core::game_context_t& game) noexcept
        : carrot::core::game_runtime_t(game)
    {
    }

    void sandbox_game_t::configure_fallback_input_actions()
    {
        _input.clear();

        _input.bind(k_input_actions.move_up, carrot::input::key_code::w);
        _input.bind(k_input_actions.move_up, carrot::input::key_code::up);
        _input.bind_gamepad_button(k_input_actions.move_up, carrot::input::gamepad_button_t::dpad_up);
        _input.bind_gamepad_axis(k_input_actions.move_up,
                                 carrot::input::gamepad_axis_t::left_y,
                                 carrot::input::gamepad_axis_direction_t::negative);
        _input.bind(k_input_actions.move_down, carrot::input::key_code::s);
        _input.bind(k_input_actions.move_down, carrot::input::key_code::down);
        _input.bind_gamepad_button(k_input_actions.move_down, carrot::input::gamepad_button_t::dpad_down);
        _input.bind_gamepad_axis(k_input_actions.move_down,
                                 carrot::input::gamepad_axis_t::left_y,
                                 carrot::input::gamepad_axis_direction_t::positive);
        _input.bind(k_input_actions.move_left, carrot::input::key_code::a);
        _input.bind(k_input_actions.move_left, carrot::input::key_code::left);
        _input.bind_gamepad_button(k_input_actions.move_left, carrot::input::gamepad_button_t::dpad_left);
        _input.bind_gamepad_axis(k_input_actions.move_left,
                                 carrot::input::gamepad_axis_t::left_x,
                                 carrot::input::gamepad_axis_direction_t::negative);
        _input.bind(k_input_actions.move_right, carrot::input::key_code::d);
        _input.bind(k_input_actions.move_right, carrot::input::key_code::right);
        _input.bind_gamepad_button(k_input_actions.move_right, carrot::input::gamepad_button_t::dpad_right);
        _input.bind_gamepad_axis(k_input_actions.move_right,
                                 carrot::input::gamepad_axis_t::left_x,
                                 carrot::input::gamepad_axis_direction_t::positive);

        _input.bind(k_input_actions.interact, carrot::input::key_code::e);
        _input.bind_gamepad_button(k_input_actions.interact, carrot::input::gamepad_button_t::south);
        _input.bind(k_input_actions.quit, carrot::input::key_code::escape);
        _input.bind(k_input_actions.toggle_fullscreen, carrot::input::key_code::f11);
        _input.bind(k_input_actions.toggle_fullscreen,
                    carrot::input::key_code::enter,
                    static_cast<uint8_t>(carrot::input::modifier::alt));
        _input.bind(k_input_actions.toggle_map_collision_debug, carrot::input::key_code::f2);
        _input.bind(k_input_actions.toggle_object_collision_debug, carrot::input::key_code::f3);
        _input.bind(k_input_actions.toggle_trigger_volume_debug, carrot::input::key_code::f4);
        _input.bind(k_input_actions.ui_up, carrot::input::key_code::i);
        _input.bind(k_input_actions.ui_down, carrot::input::key_code::k);
        _input.bind(k_input_actions.ui_left, carrot::input::key_code::j);
        _input.bind(k_input_actions.ui_right, carrot::input::key_code::l);
        _input.bind(k_input_actions.ui_accept, carrot::input::key_code::o);
        _input.bind(k_input_actions.ui_cancel, carrot::input::key_code::p);
    }

    void sandbox_game_t::configure_default_input_actions()
    {
        if (_input_binding_store.initialize(game().assets.vfs(),
                                           _input.actions(),
                                           k_input_bindings_config_path,
                                           k_user_input_bindings_config_path,
                                           k_input_action_catalog))
        {
            LOG_CORE_INFO("Loaded {} input bindings from '{}'",
                          _input_binding_store.has_user_bindings() ? "persisted" : "default",
                          _input_binding_store.has_user_bindings() ? k_user_input_bindings_config_path
                                                                   : k_input_bindings_config_path);
            ensure_action_has_default_binding(_input_binding_store,
                                              k_input_actions.toggle_map_collision_debug,
                                              "toggle_map_collision_debug");
            ensure_action_has_default_binding(_input_binding_store,
                                              k_input_actions.toggle_object_collision_debug,
                                              "toggle_object_collision_debug");
            ensure_action_has_default_binding(_input_binding_store,
                                              k_input_actions.toggle_trigger_volume_debug,
                                              "toggle_trigger_volume_debug");
            return;
        }

        LOG_CORE_WARN("Falling back to built-in sandbox input bindings");
        configure_fallback_input_actions();
    }

    void sandbox_game_t::configure_input_routing() noexcept
    {
        // _input.configure_routing(carrot::input::make_fixed_local_multiplayer_routing_config(2u));
        _input.configure_routing(carrot::input::make_single_player_routing_config());
        LOG_CORE_INFO("Sandbox input routing configured: {}", _input.describe_routing());
    }

    void sandbox_game_t::bootstrap_runtime_ui() noexcept
    {
        if (carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() })
        {
            ui_module->set_feedback_callback([](const carrot::ui::ui_feedback_event_t event)
            {
                LOG_UI_INFO("UI feedback: {}", ui_feedback_event_to_string(event));
            });

            carrot::ui::ui_root_widget_t* root{ ui_module->get_root() };
            if (!root)
                return;

            root->remove_all_children();
        }
    }

    void sandbox_game_t::start()
    {
        configure_default_input_actions();
        configure_input_routing();
        bootstrap_runtime_ui();
        set_active_state(std::make_unique<gameplay_state_t>(*this, _input));
    }

    void sandbox_game_t::tick(const float delta_time)
    {
        carrot::core::game_runtime_t::tick(delta_time);

        if (_input_rebind_session.listening() &&
            _input_rebind_session.update_gamepad_state(game().controllers.active_gamepad()))
        {
            finish_pending_input_rebind();
        }

        _rebind_status_seconds = std::max(0.f, _rebind_status_seconds - std::max(0.f, delta_time));

        if (_input_rebind_session.listening())
        {
            carrot::debug::text_colored(16.f,
                                        128.f,
                                        0xFF55FFFFu,
                                        "Rebinding '%s': press a key, gamepad button, or axis. Esc cancels.",
                                        k_input_actions.interact.authored_id.data());
        }
        else if (_rebind_status_seconds > 0.f && !_rebind_status_message.empty())
        {
            carrot::debug::text_colored(16.f,
                                        128.f,
                                        0xFF88FF88u,
                                        "%s",
                                        _rebind_status_message.c_str());
        }

        if (_input.was_just_pressed(k_input_actions.quit))
            quit_application();
    }

    void sandbox_game_t::on_key(const carrot::events::key_event_t& e)
    {
        if (e._action == carrot::events::key_action::press && !e._repeat && e._key == carrot::input::key_code::f6)
        {
            begin_interact_rebind();
            return;
        }

        if (e._action == carrot::events::key_action::press && !e._repeat && e._key == carrot::input::key_code::f7)
        {
            restore_default_input_bindings();
            return;
        }

        if (_input_rebind_session.listening())
        {
            if (e._action == carrot::events::key_action::press && !e._repeat && e._key == carrot::input::key_code::escape)
            {
                _input_rebind_session.cancel();
                _input_rebind_session.reset();
                set_rebind_status("Interact rebind cancelled.");
                return;
            }

            if (_input_rebind_session.handle_key_event(e))
            {
                finish_pending_input_rebind();
                return;
            }

            return;
        }

        const bool toggle_fullscreen{ _input.handle_key_event(e, k_input_profile, false) };
        carrot::core::game_runtime_t::on_key(e);

        if (toggle_fullscreen)
            set_fullscreen(!is_fullscreen());
    }

    void sandbox_game_t::on_mouse_moved(const carrot::events::mouse_moved_event_t& e)
    {
        carrot::core::game_runtime_t::on_mouse_moved(e);

        static int move_counter = 0;
        if (++move_counter % 5 == 0)
        {
            LOG_CORE_TRACE("Mouse: {:.0f}, {:.0f} (delta {:.1f}, {:.1f})",
                           e._pos.x,
                           e._pos.y,
                           e._delta.x,
                           e._delta.y);
        }
    }

    void sandbox_game_t::on_mouse_button(const carrot::events::mouse_button_event_t& e)
    {
        carrot::core::game_runtime_t::on_mouse_button(e);

        if (e._action == carrot::events::key_action::press)
            LOG_CORE_INFO("Mouse Button {} ({}) pressed",
                          carrot::input::mouse_button_to_string(e._button),
                          static_cast<uint32_t>(e._button));
        else if (e._action == carrot::events::key_action::release)
            LOG_CORE_INFO("Mouse Button {} ({}) released",
                          carrot::input::mouse_button_to_string(e._button),
                          static_cast<uint32_t>(e._button));
    }

    void sandbox_game_t::on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e)
    {
        carrot::core::game_runtime_t::on_mouse_scrolled(e);

        LOG_CORE_INFO("Mouse wheel scrolled: {} {}",
                      static_cast<int32_t>(e._delta.x),
                      static_cast<int32_t>(e._delta.y));
    }

    void sandbox_game_t::begin_interact_rebind()
    {
        _input_rebind_session.begin({
            .action = k_input_actions.interact,
            .allow_keys = true,
            .allow_gamepad_buttons = true,
            .allow_gamepad_axes = true,
            .allow_modifier_keys = false,
            .replace_existing_bindings = true,
            .gamepad_axis_capture_threshold = 0.5f,
        });
        LOG_CORE_INFO("Listening for a new '{}' binding. Press Escape to cancel.", k_input_actions.interact.authored_id);
    }

    void sandbox_game_t::finish_pending_input_rebind()
    {
        const std::optional<carrot::input::input_rebind_capture_t> capture{ _input_rebind_session.capture() };
        if (!capture.has_value())
            return;

        const std::string binding_description{ describe_binding(capture->binding) };
        const std::vector<carrot::input::input_binding_conflict_t> conflicts{
            _input_binding_store.find_conflicts(capture->binding, capture->action.id)
        };
        for (const carrot::input::input_binding_conflict_t& conflict : conflicts)
        {
            const std::string_view conflict_name{
                conflict.definition.has_value() ? conflict.definition->display_name : conflict.action.authored_id
            };
            LOG_CORE_WARN("Captured binding {} for '{}' conflicts with existing binding on '{}'",
                          binding_description,
                          capture->action.authored_id,
                          conflict_name);
        }

        if (!_input_rebind_session.apply_capture(_input.actions()))
        {
            _input_rebind_session.reset();
            set_rebind_status("Failed to apply the captured interact binding.");
            return;
        }

        (void)_input_rebind_session.consume_capture();
        if (!_input_binding_store.save_user_bindings())
        {
            set_rebind_status("Applied interact binding, but failed to save it.");
            LOG_CORE_WARN("Applied '{}' -> {}, but failed to persist it to '{}'",
                          capture->action.authored_id,
                          binding_description,
                          k_user_input_bindings_config_path);
            return;
        }

        set_rebind_status(std::string{ "Saved '" } +
                          std::string{ capture->action.authored_id } +
                          "' -> " +
                          binding_description +
                          ".",
                          5.f);
        LOG_CORE_INFO("Saved '{}' -> {} to '{}'",
                      capture->action.authored_id,
                      binding_description,
                      k_user_input_bindings_config_path);
    }

    void sandbox_game_t::restore_default_input_bindings()
    {
        if (!_input_binding_store.reset_to_defaults())
        {
            set_rebind_status("Failed to restore default input bindings.");
            return;
        }

        if (!_input_binding_store.save_user_bindings())
        {
            set_rebind_status("Restored defaults in memory, but failed to save them.");
            return;
        }

        _input_rebind_session.reset();
        set_rebind_status("Restored and saved default input bindings.", 5.f);
        LOG_CORE_INFO("Restored default input bindings and saved them to '{}'", k_user_input_bindings_config_path);
    }

    void sandbox_game_t::set_rebind_status(std::string message, const float seconds) noexcept
    {
        _rebind_status_message = std::move(message);
        _rebind_status_seconds = std::max(0.f, seconds);
    }
} // namespace sandbox
