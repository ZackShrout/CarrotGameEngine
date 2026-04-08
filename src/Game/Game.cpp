//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Game.h"

#include "Assets/Scene/SceneAsset.h"
#include "SandboxSceneBootstrap.h"
#include "SceneHelpers.h"
#include "TransitionRuntimeState.h"
#include "World/TriggerQuery.h"

namespace sandbox {
    namespace {
        carrot::audio::voice_handle_t handle;

        constexpr std::string_view k_action_move_up{ "move_up" };
        constexpr std::string_view k_action_move_down{ "move_down" };
        constexpr std::string_view k_action_move_left{ "move_left" };
        constexpr std::string_view k_action_move_right{ "move_right" };
        constexpr std::string_view k_action_interact{ "interact" };
        constexpr std::string_view k_action_quit{ "quit" };
        constexpr std::string_view k_action_toggle_fullscreen{ "toggle_fullscreen" };
        constexpr std::string_view k_action_toggle_map_collision_debug{ "toggle_map_collision_debug" };
        constexpr std::string_view k_action_toggle_object_collision_debug{ "toggle_object_collision_debug" };
        constexpr std::string_view k_action_ui_up{ "ui_up" };
        constexpr std::string_view k_action_ui_down{ "ui_down" };
        constexpr std::string_view k_action_ui_left{ "ui_left" };
        constexpr std::string_view k_action_ui_right{ "ui_right" };
        constexpr std::string_view k_action_ui_accept{ "ui_accept" };
        constexpr std::string_view k_action_ui_cancel{ "ui_cancel" };
        constexpr std::string_view k_input_bindings_config_path{ "game://config/input_actions.json" };

        [[nodiscard]] float apply_dead_zone_axis(const float current,
                                                 const float target,
                                                 const float half_dead_zone) noexcept
        {
            if (half_dead_zone <= 0.f)
                return target;

            const float delta{ target - current };
            if (std::fabs(delta) <= half_dead_zone)
                return current;

            return target - (std::signbit(delta) ? -half_dead_zone : half_dead_zone);
        }

        [[nodiscard]] carrot::world::world_object_t* find_object_by_id(carrot::world::world_t& world,
                                                                       const carrot::world::world_object_id_t object_id) noexcept
        {
            for (carrot::world::world_object_t& object : world.objects())
            {
                if (object.id == object_id)
                    return &object;
            }

            return nullptr;
        }

        [[nodiscard]] chlm::float2 digital_move_vector(const carrot::input::input_action_map_t& actions,
                                                       const bool suppress_up,
                                                       const bool suppress_down,
                                                       const bool suppress_left,
                                                       const bool suppress_right) noexcept
        {
            chlm::float2 movement{ 0.f, 0.f };
            if (actions.is_pressed(k_action_move_up) && !suppress_up)
                movement.y -= 1.f;
            if (actions.is_pressed(k_action_move_down) && !suppress_down)
                movement.y += 1.f;
            if (actions.is_pressed(k_action_move_left) && !suppress_left)
                movement.x -= 1.f;
            if (actions.is_pressed(k_action_move_right) && !suppress_right)
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

        [[nodiscard]] chlm::float2 resolve_move_intent(const carrot::core::game_context_t& game,
                                                       const carrot::input::input_action_map_t& actions,
                                                       const bool suppress_up,
                                                       const bool suppress_down,
                                                       const bool suppress_left,
                                                       const bool suppress_right) noexcept
        {
            if (const carrot::input::gamepad_state_t* gamepad{ game.controllers.active_gamepad() })
            {
                const chlm::float2 left_stick{ gamepad->left_stick() };
                const float length_sq{ (left_stick.x * left_stick.x) + (left_stick.y * left_stick.y) };
                if (length_sq > 0.f)
                    return left_stick;
            }

            return digital_move_vector(actions, suppress_up, suppress_down, suppress_left, suppress_right);
        }

        [[nodiscard]] bool consume_gamepad_repeat(const bool pressed,
                                                  const float delta_time,
                                                  ui_gamepad_repeat_state_t& state,
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
    } // anonymous namespace

    void sandbox_t::configure_fallback_input_actions()
    {
        _actions.clear();

        _actions.bind(std::string{ k_action_move_up }, carrot::input::key_code::w);
        _actions.bind(std::string{ k_action_move_up }, carrot::input::key_code::up);
        _actions.bind_gamepad_button(std::string{ k_action_move_up }, carrot::input::gamepad_button_t::dpad_up);
        _actions.bind_gamepad_axis(std::string{ k_action_move_up },
                                   carrot::input::gamepad_axis_t::left_y,
                                   carrot::input::gamepad_axis_direction_t::negative);
        _actions.bind(std::string{ k_action_move_down }, carrot::input::key_code::s);
        _actions.bind(std::string{ k_action_move_down }, carrot::input::key_code::down);
        _actions.bind_gamepad_button(std::string{ k_action_move_down }, carrot::input::gamepad_button_t::dpad_down);
        _actions.bind_gamepad_axis(std::string{ k_action_move_down },
                                   carrot::input::gamepad_axis_t::left_y,
                                   carrot::input::gamepad_axis_direction_t::positive);
        _actions.bind(std::string{ k_action_move_left }, carrot::input::key_code::a);
        _actions.bind(std::string{ k_action_move_left }, carrot::input::key_code::left);
        _actions.bind_gamepad_button(std::string{ k_action_move_left }, carrot::input::gamepad_button_t::dpad_left);
        _actions.bind_gamepad_axis(std::string{ k_action_move_left },
                                   carrot::input::gamepad_axis_t::left_x,
                                   carrot::input::gamepad_axis_direction_t::negative);
        _actions.bind(std::string{ k_action_move_right }, carrot::input::key_code::d);
        _actions.bind(std::string{ k_action_move_right }, carrot::input::key_code::right);
        _actions.bind_gamepad_button(std::string{ k_action_move_right }, carrot::input::gamepad_button_t::dpad_right);
        _actions.bind_gamepad_axis(std::string{ k_action_move_right },
                                   carrot::input::gamepad_axis_t::left_x,
                                   carrot::input::gamepad_axis_direction_t::positive);

        _actions.bind(std::string{ k_action_interact }, carrot::input::key_code::e);
        _actions.bind_gamepad_button(std::string{ k_action_interact }, carrot::input::gamepad_button_t::south);
        _actions.bind(std::string{ k_action_quit }, carrot::input::key_code::escape);
        _actions.bind(std::string{ k_action_toggle_fullscreen }, carrot::input::key_code::f11);
        _actions.bind(std::string{ k_action_toggle_fullscreen },
                      carrot::input::key_code::enter,
                      static_cast<uint8_t>(carrot::input::modifier::alt));
        _actions.bind(std::string{ k_action_toggle_map_collision_debug }, carrot::input::key_code::f2);
        _actions.bind(std::string{ k_action_toggle_object_collision_debug }, carrot::input::key_code::f3);
        _actions.bind(std::string{ k_action_ui_up }, carrot::input::key_code::i);
        _actions.bind(std::string{ k_action_ui_down }, carrot::input::key_code::k);
        _actions.bind(std::string{ k_action_ui_left }, carrot::input::key_code::j);
        _actions.bind(std::string{ k_action_ui_right }, carrot::input::key_code::l);
        _actions.bind(std::string{ k_action_ui_accept }, carrot::input::key_code::o);
        _actions.bind(std::string{ k_action_ui_cancel }, carrot::input::key_code::p);
    }

    void sandbox_t::configure_default_input_actions()
    {
        if (!_game)
        {
            configure_fallback_input_actions();
            return;
        }

        if (_actions.load_bindings_from_file(_game->assets.vfs(), k_input_bindings_config_path))
        {
            LOG_CORE_INFO("Loaded input bindings from '{}'", k_input_bindings_config_path);
            return;
        }

        LOG_CORE_WARN("Falling back to built-in sandbox input bindings");
        configure_fallback_input_actions();
    }

    void sandbox_t::apply_scene_camera_defaults() noexcept
    {
        if (!_game || _current_scene_id.empty())
            return;

        const carrot::assets::scene_asset_record_t* scene{ _game->assets.scenes().registry().find(_current_scene_id) };
        if (!scene)
            return;

        _game->view.set_zoom(scene->scene.camera.zoom);
        _camera_follow_mode = scene->scene.camera.follow_mode;
        _camera_initial_target_policy = scene->scene.camera.initial_target_policy;
        _camera_dead_zone_size_world = {
            std::max(0.f, scene->scene.camera.dead_zone_size_world.x),
            std::max(0.f, scene->scene.camera.dead_zone_size_world.y)
        };
        _camera_follow_smoothing = std::max(0.f, scene->scene.camera.follow_smoothing);
    }

    void sandbox_t::center_camera_on_initial_target() noexcept
    {
        if (!_game)
            return;

        if (_camera_initial_target_policy == carrot::assets::scene_camera_initial_target_policy_t::spawn_marker)
        {
            const carrot::world::world_object_t* marker{ find_marker(_game->world, _current_spawn_marker) };
            if (marker && marker->transform)
            {
                _game->view.set_center_world_position(_game->world, marker->transform->position);
                return;
            }
        }

        if (_player_controller.controlled_object() && _player_controller.controlled_object()->transform)
        {
            _game->view.set_center_world_position(_game->world, _player_controller.controlled_object()->transform->position);
        }
    }

    void sandbox_t::update_camera_follow(const float delta_time) noexcept
    {
        if (!_game || _camera_follow_mode != carrot::assets::scene_camera_follow_mode_t::player)
            return;

        if (_player_controller.controlled_object() && _player_controller.controlled_object()->transform)
        {
            const chlm::float2 current_center{ _game->view.center_world_position(_game->world) };
            const chlm::float2 target_position{ _player_controller.controlled_object()->transform->position };
            const chlm::float2 half_dead_zone{
                _camera_dead_zone_size_world.x * 0.5f,
                _camera_dead_zone_size_world.y * 0.5f
            };

            chlm::float2 desired_center{
                apply_dead_zone_axis(current_center.x, target_position.x, half_dead_zone.x),
                apply_dead_zone_axis(current_center.y, target_position.y, half_dead_zone.y)
            };

            if (_camera_follow_smoothing > 0.f && delta_time > 0.f)
            {
                const float alpha{ 1.f - std::exp(-_camera_follow_smoothing * delta_time) };
                desired_center = {
                    current_center.x + ((desired_center.x - current_center.x) * alpha),
                    current_center.y + ((desired_center.y - current_center.y) * alpha)
                };
            }

            _game->view.set_center_world_position(_game->world, desired_center);
        }
    }

    void sandbox_t::refresh_scene_bindings() noexcept
    {
        if (!_game)
            return;

        _active_trigger_ids.clear();
        _pending_trigger_events.clear();
        _player_controller.set_controlled_object(find_player(_game->world));
        _interaction_controller.set_actor(_player_controller.controlled_object());
        apply_scene_camera_defaults();
        center_camera_on_initial_target();
    }

    void sandbox_t::refresh_scene_music()
    {
        if (!_game || _current_scene_id.empty())
            return;

        const carrot::assets::scene_asset_record_t* scene{ _game->assets.scenes().registry().find(_current_scene_id) };
        if (!scene)
            return;

        if (handle.is_valid())
            carrot::audio::stop(handle);

        handle = carrot::audio::voice_handle_t::invalid();
        if (!scene->scene.initial_music_id.empty())
            handle = carrot::audio::play(scene->scene.initial_music_id);
    }

    void sandbox_t::capture_transition_runtime_state() noexcept
    {
        capture_player_runtime_state(_runtime_state, _player_controller);
    }

    void sandbox_t::apply_scene_runtime_state() noexcept
    {
        if (!_game || _current_scene_id.empty())
            return;

        apply_runtime_state_to_scene(_current_scene_id, _game->world, _runtime_state);
        apply_runtime_state_to_player(_runtime_state, _player_controller);
    }

    void sandbox_t::consume_pending_runtime_events() noexcept
    {
        if (!_game || _current_scene_id.empty())
            return;

        if (const std::optional<opened_container_request_t> opened_container{ _interaction_controller.consume_pending_opened_container() })
        {
            carrot::world::world_object_t* container{ find_object_by_id(_game->world, opened_container->object_id) };
            if (!container)
            {
                LOG_CORE_WARN("Opened container request referenced missing world object id {}", opened_container->object_id);
                return;
            }

            mark_container_open(_runtime_state, _current_scene_id, *container);
            apply_runtime_state_to_scene(_current_scene_id, _game->world, _runtime_state);
            LOG_CORE_INFO("Marked container '{}' as opened in scene '{}'", container->name, _current_scene_id);
        }

        for (const trigger_event_t& event : _pending_trigger_events)
            handle_trigger_event(event);

        _pending_trigger_events.clear();
    }

    void sandbox_t::handle_trigger_event(const trigger_event_t& event) noexcept
    {
        const char* phase_label{ event.phase == trigger_event_phase_t::entered ? "Entered" : "Exited" };
        LOG_CORE_INFO("{} Trigger -> trigger_id='{}', trigger_kind='{}', object_id={}",
                      phase_label,
                      event.trigger_id,
                      event.trigger_kind,
                      event.object_id);
    }

    void sandbox_t::update_trigger_overlaps() noexcept
    {
        if (!_game || !_player_controller.controlled_object())
            return;

        const carrot::world::trigger_overlap_changes_t changes{
            carrot::world::update_trigger_overlaps(*_player_controller.controlled_object(),
                                                   _game->world,
                                                   _active_trigger_ids)
        };

        for (const carrot::world::world_object_t* trigger : changes.entered)
        {
            if (const std::optional<trigger_interaction_data_t> data{ as_trigger(*trigger) })
            {
                _pending_trigger_events.emplace_back(trigger_event_t{
                    .object_id = trigger->id,
                    .phase = trigger_event_phase_t::entered,
                    .trigger_id = std::string{ data->trigger_id },
                    .trigger_kind = std::string{ data->trigger_kind }
                });
            }
        }

        for (const carrot::world::world_object_t* trigger : changes.exited)
        {
            if (const std::optional<trigger_interaction_data_t> data{ as_trigger(*trigger) })
            {
                _pending_trigger_events.emplace_back(trigger_event_t{
                    .object_id = trigger->id,
                    .phase = trigger_event_phase_t::exited,
                    .trigger_id = std::string{ data->trigger_id },
                    .trigger_kind = std::string{ data->trigger_kind }
                });
            }
        }
    }

    void sandbox_t::toggle_map_collision_debug() noexcept
    {
        if (!_game)
            return;

        auto& debug_view{ _game->world.collision_debug_view() };
        debug_view.show_map_collision = !debug_view.show_map_collision;
        LOG_CORE_INFO("Map collision debug: {}", debug_view.show_map_collision ? "ON" : "OFF");
    }

    void sandbox_t::toggle_object_collision_debug() noexcept
    {
        if (!_game)
            return;

        auto& debug_view{ _game->world.collision_debug_view() };
        debug_view.show_object_colliders = !debug_view.show_object_colliders;
        LOG_CORE_INFO("Object collider debug: {}", debug_view.show_object_colliders ? "ON" : "OFF");
    }

    void sandbox_t::initialize_runtime_ui_probe() noexcept
    {
        carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() };
        if (!ui_module)
            return;

        carrot::ui::ui_root_widget_t* root{ ui_module->get_root() };
        if (!root)
            return;

        root->remove_all_children();

        carrot::ui::ui_stack_t& stack{ root->emplace_child<carrot::ui::ui_stack_t>(carrot::ui::ui_stack_orientation_t::vertical) };
        stack.set_padding({ .left = 24.f, .top = 24.f, .right = 24.f, .bottom = 24.f });
        stack.set_spacing(12.f);
        stack.set_cross_alignment(carrot::ui::ui_stack_cross_alignment_t::start);

        carrot::ui::ui_button_t& item_a{ stack.emplace_child<carrot::ui::ui_button_t>("Start Game") };
        carrot::ui::ui_button_t& item_b{ stack.emplace_child<carrot::ui::ui_button_t>("Options") };
        carrot::ui::ui_button_t& item_c{ stack.emplace_child<carrot::ui::ui_button_t>("Exit") };

        item_a.set_on_focused([]() { LOG_UI_INFO("UI Probe 'Start Game' focused"); });
        item_a.set_on_focus_lost([]() { LOG_UI_INFO("UI Probe 'Start Game' focus lost"); });
        item_a.set_on_pressed([]() { LOG_UI_INFO("UI Probe 'Start Game' accepted"); });
        item_a.set_on_canceled([]() { LOG_UI_INFO("UI Probe 'Start Game' canceled"); });

        item_b.set_on_focused([]() { LOG_UI_INFO("UI Probe 'Options' focused"); });
        item_b.set_on_focus_lost([]() { LOG_UI_INFO("UI Probe 'Options' focus lost"); });
        item_b.set_on_pressed([]() { LOG_UI_INFO("UI Probe 'Options' accepted"); });
        item_b.set_on_canceled([]() { LOG_UI_INFO("UI Probe 'Options' canceled"); });

        item_c.set_on_focused([]() { LOG_UI_INFO("UI Probe 'Exit' focused"); });
        item_c.set_on_focus_lost([]() { LOG_UI_INFO("UI Probe 'Exit' focus lost"); });
        item_c.set_on_pressed([]() { LOG_UI_INFO("UI Probe 'Exit' accepted"); });
        item_c.set_on_canceled([]() { LOG_UI_INFO("UI Probe 'Exit' canceled"); });

        item_a.set_navigation_target(carrot::ui::ui_navigation_direction_t::down, &item_b);
        item_b.set_navigation_target(carrot::ui::ui_navigation_direction_t::up, &item_a);
        item_b.set_navigation_target(carrot::ui::ui_navigation_direction_t::down, &item_c);
        item_c.set_navigation_target(carrot::ui::ui_navigation_direction_t::up, &item_b);

        const bool focused{ ui_module->focus_first() };
        LOG_UI_INFO("Runtime UI probe initialized (focus set: {})", focused ? "yes" : "no");
    }

    void sandbox_t::update_runtime_ui_navigation(const float delta_time) noexcept
    {
        carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() };
        if (!ui_module)
            return;

        const bool up_pressed{ _actions.is_pressed_by_gamepad(k_action_ui_up) };
        const bool down_pressed{ _actions.is_pressed_by_gamepad(k_action_ui_down) };
        const bool left_pressed{ _actions.is_pressed_by_gamepad(k_action_ui_left) };
        const bool right_pressed{ _actions.is_pressed_by_gamepad(k_action_ui_right) };
        const bool accept_pressed{ _actions.is_pressed_by_gamepad(k_action_ui_accept) };
        const bool cancel_pressed{ _actions.is_pressed_by_gamepad(k_action_ui_cancel) };

        if (consume_gamepad_repeat(up_pressed,
                                   delta_time,
                                   _ui_up_gamepad_repeat,
                                   _ui_gamepad_nav_repeat_initial_delay_seconds,
                                   _ui_gamepad_nav_repeat_interval_seconds))
            (void)ui_module->dispatch_navigation_action(k_action_ui_up);
        if (consume_gamepad_repeat(down_pressed,
                                   delta_time,
                                   _ui_down_gamepad_repeat,
                                   _ui_gamepad_nav_repeat_initial_delay_seconds,
                                   _ui_gamepad_nav_repeat_interval_seconds))
            (void)ui_module->dispatch_navigation_action(k_action_ui_down);
        if (consume_gamepad_repeat(left_pressed,
                                   delta_time,
                                   _ui_left_gamepad_repeat,
                                   _ui_gamepad_nav_repeat_initial_delay_seconds,
                                   _ui_gamepad_nav_repeat_interval_seconds))
            (void)ui_module->dispatch_navigation_action(k_action_ui_left);
        if (consume_gamepad_repeat(right_pressed,
                                   delta_time,
                                   _ui_right_gamepad_repeat,
                                   _ui_gamepad_nav_repeat_initial_delay_seconds,
                                   _ui_gamepad_nav_repeat_interval_seconds))
            (void)ui_module->dispatch_navigation_action(k_action_ui_right);

        if (accept_pressed && !_ui_accept_gamepad_was_pressed)
            (void)ui_module->dispatch_navigation_action(k_action_ui_accept);
        if (cancel_pressed && !_ui_cancel_gamepad_was_pressed)
            (void)ui_module->dispatch_navigation_action(k_action_ui_cancel);

        _ui_accept_gamepad_was_pressed = accept_pressed;
        _ui_cancel_gamepad_was_pressed = cancel_pressed;
    }

    bool sandbox_t::load_scene(const std::string_view scene_id, const std::string_view spawn_marker)
    {
        if (!_game)
            return false;

        if (!_current_scene_id.empty())
            capture_transition_runtime_state();

        if (!bootstrap_scene(*_game, scene_id, spawn_marker))
            return false;

        _current_scene_id = std::string{ scene_id };
        _current_spawn_marker = std::string{ spawn_marker };
        if (_current_spawn_marker.empty())
        {
            if (const carrot::assets::scene_asset_record_t* scene{ _game->assets.scenes().registry().find(_current_scene_id) })
                _current_spawn_marker = scene->scene.player_spawn_marker;
        }

        refresh_scene_bindings();
        apply_scene_runtime_state();
        refresh_scene_music();
        return true;
    }

    void sandbox_t::start(carrot::core::game_context_t& game)
    {
        _game = &game;
        configure_default_input_actions();

        if (carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() })
        {
            ui_module->set_feedback_callback([](const carrot::ui::ui_feedback_event_t event)
            {
                // Placeholder bridge for future UI SFX wiring.
                // Replace each case with audio::play(...) once assets are ready.
                LOG_UI_INFO("UI feedback: {}", ui_feedback_event_to_string(event));
            });
        }

        _player_controller.set_animation_set({
            .idle_down = "idle_down",
            .idle_up = "idle_up",
            .idle_left = "idle_left",
            .idle_right = "idle_right",
            .walk_down = "walk_down",
            .walk_up = "walk_up",
            .walk_left = "walk_left",
            .walk_right = "walk_right"
        });
        _player_controller.set_move_speed(4.0f);
        _interaction_controller.set_interaction_radius(3.0f);
        initialize_runtime_ui_probe();
        load_scene(k_bootstrap_scene_id);
    }

    void sandbox_t::on_tick(const float delta_time)
    {
        if (!_game)
            return;

        _actions.update_gamepad_state(_game->controllers.active_gamepad());
        update_runtime_ui_navigation(delta_time);
        _player_controller.set_move_intent(resolve_move_intent(*_game,
                                                               _actions,
                                                               _move_up_suppressed_by_ui,
                                                               _move_down_suppressed_by_ui,
                                                               _move_left_suppressed_by_ui,
                                                               _move_right_suppressed_by_ui));

        const bool interact_pressed{ _actions.is_pressed(k_action_interact) };
        if (interact_pressed && !_interact_was_pressed)
        {
            if (!_interaction_controller.actor() || !_interaction_controller.actor()->transform)
            {
                LOG_CORE_WARN("Interaction failed: controlled player world object is missing a transform");
            }
            else if (!_interaction_controller.try_interact(*_game))
            {
                LOG_CORE_INFO("No interactable in range");
            }
        }
        _interact_was_pressed = interact_pressed;

        const bool quit_pressed{ _actions.is_pressed(k_action_quit) };
        if (quit_pressed && !_quit_was_pressed)
            quit_application();
        _quit_was_pressed = quit_pressed;

        const bool toggle_map_collision_debug_pressed{ _actions.is_pressed(k_action_toggle_map_collision_debug) };
        if (toggle_map_collision_debug_pressed && !_toggle_map_collision_debug_was_pressed)
            toggle_map_collision_debug();
        _toggle_map_collision_debug_was_pressed = toggle_map_collision_debug_pressed;

        const bool toggle_object_collision_debug_pressed{ _actions.is_pressed(k_action_toggle_object_collision_debug) };
        if (toggle_object_collision_debug_pressed && !_toggle_object_collision_debug_was_pressed)
            toggle_object_collision_debug();
        _toggle_object_collision_debug_was_pressed = toggle_object_collision_debug_pressed;

        consume_pending_runtime_events();

        if (const std::optional<scene_transition_request_t> request{ _interaction_controller.consume_pending_transition() })
            load_scene(request->scene_id, request->marker_name);

        _player_controller.update(*_game, delta_time);
        update_trigger_overlaps();
        update_camera_follow(delta_time);
    }

    void sandbox_t::on_window_focus_changed(const carrot::events::window_focused_t& e)
    {
        ce_application_t::on_window_focus_changed(e);

        if (e._focused)
            return;

        // Wayland fullscreen/state transitions can drop key-release events on focus changes.
        // Flush held key action state to prevent gameplay input from latching.
        _actions.release_all_keys();
        _player_controller.set_move_up(false);
        _player_controller.set_move_down(false);
        _player_controller.set_move_left(false);
        _player_controller.set_move_right(false);
        _interact_was_pressed = false;
        _quit_was_pressed = false;
        _toggle_map_collision_debug_was_pressed = false;
        _toggle_object_collision_debug_was_pressed = false;
        _ui_up_was_pressed = false;
        _ui_down_was_pressed = false;
        _ui_left_was_pressed = false;
        _ui_right_was_pressed = false;
        _ui_accept_was_pressed = false;
        _ui_cancel_was_pressed = false;
        _move_up_suppressed_by_ui = false;
        _move_down_suppressed_by_ui = false;
        _move_left_suppressed_by_ui = false;
        _move_right_suppressed_by_ui = false;
        _ui_up_gamepad_repeat = {};
        _ui_down_gamepad_repeat = {};
        _ui_left_gamepad_repeat = {};
        _ui_right_gamepad_repeat = {};
        _ui_accept_gamepad_was_pressed = false;
        _ui_cancel_gamepad_was_pressed = false;
    }

    void sandbox_t::on_key(const carrot::events::key_event_t& e)
    {
        ce_application_t::on_key(e);
        _actions.handle_key_event(e);

        bool ui_consumed_event{ false };
        auto handle_ui_key_action = [this, &e, &ui_consumed_event](const std::string_view action_name, bool& was_pressed_flag) noexcept
        {
            if (!_actions.matches(action_name, e))
                return false;

            if (e._action == carrot::events::key_action::press)
            {
                if (carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() })
                    ui_consumed_event = ui_module->dispatch_navigation_action(action_name) || ui_consumed_event;
                was_pressed_flag = true;
                return true;
            }

            if (e._action == carrot::events::key_action::repeat)
            {
                if (carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() })
                    ui_consumed_event = ui_module->dispatch_navigation_action(action_name) || ui_consumed_event;
                was_pressed_flag = true;
                return true;
            }

            if (e._action == carrot::events::key_action::release)
            {
                was_pressed_flag = false;
                return true;
            }

            return false;
        };

        (void)handle_ui_key_action(k_action_ui_up, _ui_up_was_pressed);
        (void)handle_ui_key_action(k_action_ui_down, _ui_down_was_pressed);
        (void)handle_ui_key_action(k_action_ui_left, _ui_left_was_pressed);
        (void)handle_ui_key_action(k_action_ui_right, _ui_right_was_pressed);
        (void)handle_ui_key_action(k_action_ui_accept, _ui_accept_was_pressed);
        (void)handle_ui_key_action(k_action_ui_cancel, _ui_cancel_was_pressed);

        auto update_move_suppression = [this, &e, ui_consumed_event](const std::string_view move_action, bool& suppressed_flag) noexcept
        {
            if (!_actions.matches(move_action, e))
                return;

            if (e._action == carrot::events::key_action::release)
            {
                suppressed_flag = false;
                return;
            }

            if (e._action == carrot::events::key_action::press || e._action == carrot::events::key_action::repeat)
            {
                if (ui_consumed_event)
                    suppressed_flag = true;
            }
        };

        update_move_suppression(k_action_move_up, _move_up_suppressed_by_ui);
        update_move_suppression(k_action_move_down, _move_down_suppressed_by_ui);
        update_move_suppression(k_action_move_left, _move_left_suppressed_by_ui);
        update_move_suppression(k_action_move_right, _move_right_suppressed_by_ui);

        if (e._action == carrot::events::key_action::press)
        {
            if (_actions.matches(k_action_toggle_fullscreen, e) && !e._repeat)
                set_fullscreen(!is_fullscreen());

            if (_actions.matches(k_action_interact, e) && _game)
            {
                LOG_CORE_INFO("Key pressed: {} ({}) (mods: {})", carrot::input::key_code_to_string(e._key),
                              static_cast<uint32_t>(e._key), carrot::input::modifiers_to_string(e._mods));
            }
        }
        else if (e._action == carrot::events::key_action::repeat &&
                 (_actions.matches(k_action_interact, e) ||
                  e._key == carrot::input::key_code::escape ||
                  e._key == carrot::input::key_code::enter ||
                  e._key == carrot::input::key_code::f11))
            LOG_CORE_INFO("Key held: {} ({})", carrot::input::key_code_to_string(e._key),
                      static_cast<uint32_t>(e._key));
        else if (e._action == carrot::events::key_action::release &&
                 (_actions.matches(k_action_interact, e) ||
                  e._key == carrot::input::key_code::escape ||
                  e._key == carrot::input::key_code::enter ||
                  e._key == carrot::input::key_code::f11))
            LOG_CORE_INFO("Key released: {} ({})", carrot::input::key_code_to_string(e._key),
                      static_cast<uint32_t>(e._key));

        _player_controller.set_move_up(_actions.is_pressed(k_action_move_up) && !_move_up_suppressed_by_ui);
        _player_controller.set_move_down(_actions.is_pressed(k_action_move_down) && !_move_down_suppressed_by_ui);
        _player_controller.set_move_left(_actions.is_pressed(k_action_move_left) && !_move_left_suppressed_by_ui);
        _player_controller.set_move_right(_actions.is_pressed(k_action_move_right) && !_move_right_suppressed_by_ui);
    }

    void sandbox_t::on_mouse_moved(const carrot::events::mouse_moved_event_t& e)
    {
        ce_application_t::on_mouse_moved(e);

        static int move_counter = 0;
        if (++move_counter % 5 == 0)
        {
            LOG_CORE_TRACE("Mouse: {:.0f}, {:.0f} (delta {:.1f}, {:.1f})", e._pos.x, e._pos.y, e._delta.x, e._delta.y);
        }
    }

    void sandbox_t::on_mouse_button(const carrot::events::mouse_button_event_t& e)
    {
        ce_application_t::on_mouse_button(e);

        if (e._action == carrot::events::key_action::press)
            LOG_CORE_INFO("Mouse Button {} ({}) pressed", carrot::input::mouse_button_to_string(e._button),
                      static_cast<uint32_t>(e._button));
        else if (e._action == carrot::events::key_action::release)
            LOG_CORE_INFO("Mouse Button {} ({}) released", carrot::input::mouse_button_to_string(e._button),
                      static_cast<uint32_t>(e._button));
    }

    void sandbox_t::on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e)
    {
        ce_application_t::on_mouse_scrolled(e);

        LOG_CORE_INFO("Mouse wheel scrolled: {} {}", static_cast<int32_t>(e._delta.x),
                      static_cast<int32_t>(e._delta.y));
    }
} // namespace sandbox
