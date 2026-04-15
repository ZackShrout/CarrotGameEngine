//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameplayState.h"

#include "SandboxInputConfig.h"
#include "World/TriggerQuery.h"

namespace sandbox {
    namespace {
        [[nodiscard]] const char* routing_mode_to_string(
            const carrot::input::gameplay_input_routing_mode_t mode) noexcept
        {
            switch (mode)
            {
                case carrot::input::gameplay_input_routing_mode_t::single_player_auto: return "single_player_auto";
                case carrot::input::gameplay_input_routing_mode_t::local_multiplayer_fixed: return
                            "local_multiplayer_fixed";
            }

            return "unknown";
        }

        [[nodiscard]] bool validate_loaded_sandbox_scene(const carrot::assets::asset_manager_t& assets,
                                                         const carrot::world::world_t& world,
                                                         const std::string_view scene_id)
        {
            size_t container_count{ 0 };

            for (const carrot::world::world_object_t* object: world.find_objects_by_type("Container"))
                if (object) ++container_count;

            const size_t door_count{ world.find_objects_by_type("Door").size() };
            const size_t sign_count{ world.find_objects_by_type("Sign").size() };

            LOG_ASSET_INFO("Sandbox scene hybrids: Container={}, Door={}, Sign={}", container_count, door_count,
                           sign_count);

            carrot::assets::asset_manager_t& mutable_assets{ const_cast<carrot::assets::asset_manager_t&>(assets) };

            const carrot::world::authored::scene_validation_report_t report{
                carrot::world::authored::build_scene_validation_report(mutable_assets, world)
            };

            if (!report.valid())
            {
                LOG_ASSET_ERROR("Scene '{}' validation failed with {} issue(s)", scene_id, report.issues.size());
                return false;
            }

            LOG_ASSET_INFO("Scene '{}' validation passed", scene_id);
            return true;
        }

        void update_world_lighting(carrot::world::world_t& world,
                                   const carrot::world::player_controller_t& player_controller) noexcept
        {
            carrot::world::world_lighting_state_t& lighting{ world.lighting() };
            lighting.ambient_color = { 0.36f, 0.38f, 0.44f, 1.f };
            lighting.point_lights.clear();

            const carrot::world::world_object_t* actor{ player_controller.controlled_object() };
            if (actor && actor->transform)
            {
                lighting.point_lights.push_back({
                    .position_world = {
                        actor->transform->position.x,
                        actor->transform->position.y - 0.15f
                    },
                    .radius_world = 3.25f,
                    .reserved0 = 0.f,
                    .color = { 1.0f, 0.82f, 0.58f, 1.f },
                    .intensity = 1.15f
                });
            }

            lighting.point_lights.push_back({
                .position_world = { 69.8f, 57.6f },
                .radius_world = 2.4f,
                .reserved0 = 0.f,
                .color = { 0.82f, 0.30f, 1.0f, 1.f },
                .intensity = 1.05f
            });

            lighting.point_lights.push_back({
                .position_world = { 72.4f, 52.9f },
                .radius_world = 2.5f,
                .reserved0 = 0.f,
                .color = { 0.20f, 0.58f, 1.0f, 1.f },
                .intensity = 1.0f
            });

            lighting.point_lights.push_back({
                .position_world = { 64.9f, 52.2f },
                .radius_world = 2.35f,
                .reserved0 = 0.f,
                .color = { 0.28f, 1.0f, 0.42f, 1.f },
                .intensity = 0.95f
            });
        }
    } // namespace

    void gameplay_state_t::enter()
    {
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
        constexpr std::string_view k_initial_scene_id{ "scene.sandbox.town" };
        (void)_scene_runtime.request_load(game(), k_initial_scene_id, make_scene_load_options());
    }

    void gameplay_state_t::before_scene_change([[maybe_unused]] carrot::core::game_context_t& game,
                                               const carrot::scene::scene_runtime_context_t* current_context,
                                               [[maybe_unused]] const std::string_view next_scene_id,
                                               [[maybe_unused]] const std::string_view next_spawn_marker)
    {
        if (!current_context)
            return;

        prepare_for_scene_change(current_context);
    }

    void gameplay_state_t::after_scene_change([[maybe_unused]] carrot::core::game_context_t& game,
                                              const carrot::scene::scene_runtime_context_t& current_context)
    {
        finalize_scene_change(current_context);
    }

    carrot::scene::scene_load_options_t gameplay_state_t::make_scene_load_options(
        const std::string_view spawn_marker_override) noexcept
    {
        return carrot::scene::scene_load_options_t{
            .spawn_marker_override = spawn_marker_override,
            .player_controller = &_player_controller,
            .interaction_controller = &_interaction_controller,
            .validate_loaded_scene = validate_loaded_sandbox_scene,
            .listener = this
        };
    }

    void gameplay_state_t::prepare_for_scene_change(
        [[maybe_unused]] const carrot::scene::scene_runtime_context_t* current_context) noexcept
    {
        capture_player_runtime_state(_runtime_state, _player_controller);
    }

    void gameplay_state_t::finalize_scene_change(const carrot::scene::scene_runtime_context_t& current_context) noexcept
    {
        _trigger_monitor.reset();
        update_world_lighting(current_context.world, _player_controller);
        apply_runtime_state_to_scene(current_context.scene_id, current_context.world, _runtime_state);
        apply_runtime_state_to_player(_runtime_state, _player_controller);
    }

    void gameplay_state_t::consume_pending_runtime_events() noexcept
    {
        if (!_scene_runtime.has_scene_loaded())
            return;

        const carrot::scene::scene_runtime_context_t context{ _scene_runtime.make_context(game()) };
        (void)_interaction_controller.dispatch_pending_interaction({
            .on_scene_transition = [this](const carrot::scene::scene_transition_request_t& request) {
                (void)transition_scene(request);
            },
            .on_container = [this, &context](const carrot::world::world_object_id_t object_id,
                                             [[maybe_unused]] const std::string_view loot_table) {
                carrot::world::world_object_t* container{ context.find_object_by_id(object_id) };
                if (!container)
                {
                    LOG_CORE_WARN("Opened container request referenced missing world object id {}", object_id);
                    return;
                }

                mark_container_open(_runtime_state, context.scene_id, *container);
                apply_runtime_state_to_scene(context.scene_id, context.world, _runtime_state);
                LOG_CORE_INFO("Marked container '{}' as opened in scene '{}'",
                              container->name,
                              context.scene_id);
            }
        });

        (void)_trigger_monitor.dispatch_pending_events({
            .on_any = [this](const carrot::world::trigger_event_t& event) {
                handle_trigger_event(event);
            }
        });
    }

    void gameplay_state_t::handle_trigger_event(const carrot::world::trigger_event_t& event) noexcept
    {
        const char* phase_label{ event.phase == carrot::world::trigger_event_phase_t::entered ? "Entered" : "Exited" };
        LOG_CORE_INFO("{} Trigger -> trigger_id='{}', trigger_kind='{}', object_id={}",
                      phase_label,
                      event.trigger_id,
                      event.trigger_kind,
                      event.object_id);
    }

    void gameplay_state_t::toggle_map_collision_debug() noexcept
    {
        auto& debug_view{ game().world.collision_debug_view() };
        debug_view.show_map_collision = !debug_view.show_map_collision;
        LOG_CORE_INFO("Map collision debug: {}", debug_view.show_map_collision ? "ON" : "OFF");
    }

    void gameplay_state_t::toggle_object_collision_debug() noexcept
    {
        auto& debug_view{ game().world.collision_debug_view() };
        debug_view.show_object_colliders = !debug_view.show_object_colliders;
        LOG_CORE_INFO("Object collider debug: {}", debug_view.show_object_colliders ? "ON" : "OFF");
    }

    bool gameplay_state_t::load_scene(const std::string_view scene_id, const std::string_view spawn_marker)
    {
        return _scene_runtime.request_load(game(), scene_id, make_scene_load_options(spawn_marker));
    }

    bool gameplay_state_t::transition_scene(const carrot::scene::scene_transition_request_t& request)
    {
        return _scene_runtime.request_transition(game(), request, make_scene_load_options());
    }

    void gameplay_state_t::tick(const float delta_time)
    {
        _input.update(game(), k_input_profile, delta_time);
        _input.apply_player_movement(_player_controller, game(), k_input_profile);
        (void)_input.dispatch_interaction_if_triggered(_interaction_controller, game(), k_input_profile);

        if (_input.was_just_pressed(k_input_actions.toggle_map_collision_debug))
            toggle_map_collision_debug();

        if (_input.was_just_pressed(k_input_actions.toggle_object_collision_debug))
            toggle_object_collision_debug();

        (void)_scene_runtime.update(game());
        consume_pending_runtime_events();

        _player_controller.update(game(), delta_time);
        update_world_lighting(game().world, _player_controller);
        if (const carrot::world::world_object_t* actor{ _player_controller.controlled_object() })
            _trigger_monitor.update(*actor, game().world);
        _scene_runtime.update_camera(game(), delta_time);

        if (_input.routing_mode() == carrot::input::gameplay_input_routing_mode_t::local_multiplayer_fixed)
        {
            carrot::debug::text_colored(16.f,
                                        156.f,
                                        0xFF9AD0FFu,
                                        "Routing: %s | Players: %zu",
                                        routing_mode_to_string(_input.routing_mode()),
                                        _input.player_count());

            for (size_t player_index{ 0u }; player_index < std::min<size_t>(_input.player_count(), 2u); ++player_index)
            {
                const carrot::input::player_input_context_t* context{ _input.player(player_index) };
                const chlm::float2 intent{ _input.movement_intent(player_index, game(), k_input_profile) };
                carrot::debug::text_colored(16.f,
                                            184.f + (28.f * static_cast<float>(player_index)),
                                            player_index == 0u ? 0xFF88FF88u : 0xFFFFC888u,
                                            "P%zu keyboard=%s gamepad=%s move=(%.2f, %.2f) interact=%s",
                                            player_index + 1u,
                                            context && context->receives_keyboard() ? "yes" : "no",
                                            context && context->gamepad_slot().has_value()
                                                ? std::to_string(*context->gamepad_slot()).c_str()
                                                : "none",
                                            intent.x,
                                            intent.y,
                                            _input.is_pressed(player_index, k_input_actions.interact) ? "down" : "up");
            }
        }
    }

    void gameplay_state_t::on_window_focus_changed(const carrot::events::window_focused_t& e)
    {
        if (e._focused)
            return;

        _input.on_focus_lost();
        _player_controller.set_move_up(false);
        _player_controller.set_move_down(false);
        _player_controller.set_move_left(false);
        _player_controller.set_move_right(false);
    }

    void gameplay_state_t::on_key(const carrot::events::key_event_t& e)
    {
        _input.apply_player_movement(_player_controller, game(), k_input_profile);
    }
} // namespace sandbox
