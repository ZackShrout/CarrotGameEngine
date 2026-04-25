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
        _scene_runtime.set_default_runtime_bindings(make_scene_runtime_bindings());
        constexpr std::string_view k_initial_scene_id{ "scene.sandbox.town" };
        (void)_scene_runtime.request_load(game(), k_initial_scene_id);
    }

    void gameplay_state_t::before_scene_change([[maybe_unused]] carrot::core::game_context_t& game,
                                               const carrot::scene::scene_runtime_context_t* current_context,
                                               [[maybe_unused]] const std::string_view next_scene_id,
                                               [[maybe_unused]] const std::string_view next_spawn_marker)
    {
        if (_applying_loaded_runtime_state)
            return;

        if (!current_context)
            return;

        prepare_for_scene_change(current_context);
    }

    void gameplay_state_t::after_scene_change([[maybe_unused]] carrot::core::game_context_t& game,
                                              const carrot::scene::scene_runtime_context_t& current_context)
    {
        finalize_scene_change(current_context);
    }

    carrot::scene::scene_runtime_bindings_t gameplay_state_t::make_scene_runtime_bindings() noexcept
    {
        return carrot::scene::scene_runtime_bindings_t{
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
        const bool visible{ runtime().toggle_map_collision_debug() };
        LOG_CORE_INFO("Map collision debug: {}", visible ? "ON" : "OFF");
    }

    void gameplay_state_t::toggle_object_collision_debug() noexcept
    {
        const bool visible{ runtime().toggle_object_collider_debug() };
        LOG_CORE_INFO("Object collider debug: {}", visible ? "ON" : "OFF");
    }

    void gameplay_state_t::toggle_trigger_volume_debug() noexcept
    {
        const bool visible{ runtime().toggle_trigger_volume_debug() };
        LOG_CORE_INFO("Trigger volume debug: {}", visible ? "ON" : "OFF");
    }

    bool gameplay_state_t::load_scene(const std::string_view scene_id, const std::string_view spawn_marker)
    {
        return _scene_runtime.request_load(game(),
                                          scene_id,
                                          carrot::scene::make_scene_load_options({}, spawn_marker));
    }

    bool gameplay_state_t::transition_scene(const carrot::scene::scene_transition_request_t& request)
    {
        carrot::scene::scene_load_options_t options{ };
        const std::string_view current_scene_id{ _scene_runtime.current_scene_id() };
        const bool use_battle_swirl{
            request.scene_id == "scene.sandbox.item_shop" ||
            (current_scene_id == "scene.sandbox.item_shop" && request.scene_id == "scene.sandbox.town")
        };
        if (use_battle_swirl)
            options.transition_overlay.effect = carrot::scene::scene_transition_effect_t::battle_swirl;

        return _scene_runtime.request_transition(game(), request, options);
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

        if (_input.was_just_pressed(k_input_actions.toggle_trigger_volume_debug))
            toggle_trigger_volume_debug();

        (void)_scene_runtime.update(game());
        consume_pending_runtime_events();

        _player_controller.update(game(), delta_time);
        if (const carrot::world::world_object_t* actor{ _player_controller.controlled_object() })
            _trigger_monitor.update(*actor, game().world);
        _scene_runtime.update_camera(game(), delta_time);

        if (_input.routing_mode() == carrot::input::gameplay_input_routing_mode_t::local_multiplayer_fixed)
        {
            carrot::debug::text_colored(16.f,
                                        156.f,
                                        0xFF9AD0FFu,
                                        "Routing: %s | Players: %zu",
                                        carrot::input::to_string(_input.routing_mode()).data(),
                                        _input.player_count());

            for (size_t player_index{ 0u }; player_index < std::min<size_t>(_input.player_count(), 2u); ++player_index)
            {
                const carrot::input::player_input_context_t* context{ _input.player(player_index) };
                const chlm::float2 intent{ _input.movement_intent(player_index, game(), k_input_profile) };
                const std::string context_summary{
                    context ? carrot::input::describe_player_input_context(*context) : "missing player context"
                };
                carrot::debug::text_colored(16.f,
                                            184.f + (28.f * static_cast<float>(player_index)),
                                            player_index == 0u ? 0xFF88FF88u : 0xFFFFC888u,
                                            "%s move=(%.2f, %.2f) interact=%s",
                                            context_summary.c_str(),
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
        _player_controller.clear_movement_intent();
    }

    void gameplay_state_t::on_key(const carrot::events::key_event_t& e)
    {
        _input.apply_player_movement(_player_controller, game(), k_input_profile);
    }

    void gameplay_state_t::register_save_participants(carrot::save::save_participant_registry_t& registry)
    {
        registry.add(_save_participant);
    }

    bool gameplay_state_t::gameplay_save_participant_t::capture_save_sections(
        const carrot::save::save_request_t& request,
        carrot::save::save_section_collector_t& collector,
        carrot::save::save_operation_status_t& status)
    {
        (void)request;

        if (!_owner._scene_runtime.has_scene_loaded())
        {
            status.detail = "Sandbox gameplay save requires an active scene.";
            return false;
        }

        capture_player_runtime_state(_owner._runtime_state, _owner._player_controller);

        const gameplay_durable_state_t durable_state{
            .scene_id = std::string{ _owner._scene_runtime.current_scene_id() },
            .spawn_marker = std::string{ _owner._scene_runtime.current_spawn_marker() },
            .runtime_state = _owner._runtime_state
        };
        std::optional<std::vector<std::uint8_t>> bytes{ serialize_durable_state(durable_state) };
        if (!bytes.has_value())
        {
            status.detail = "Sandbox gameplay durable state could not be serialized.";
            return false;
        }

        if (!collector.add_section("sandbox_gameplay_state", owner(), *bytes, status.detail))
            return false;

        status.detail = "Captured sandbox gameplay durable state.";
        return true;
    }

    bool gameplay_state_t::gameplay_save_participant_t::apply_loaded_sections(
        const carrot::save::loaded_save_slot_t& slot,
        carrot::save::save_operation_status_t& status)
    {
        const carrot::save::save_payload_section_t* section{ slot.find_section("sandbox_gameplay_state") };
        if (!section)
        {
            status.detail = "Save slot is missing the sandbox gameplay durable state section.";
            return false;
        }

        if (section->owner != owner())
        {
            status.detail = "Sandbox gameplay durable state section owner mismatch.";
            return false;
        }

        const std::optional<gameplay_durable_state_t> durable_state{ deserialize_durable_state(section->bytes) };
        if (!durable_state.has_value())
        {
            status.detail = "Sandbox gameplay durable state could not be deserialized.";
            return false;
        }

        _owner._runtime_state = durable_state->runtime_state;
        _owner._applying_loaded_runtime_state = true;
        const bool loaded{
            _owner._scene_runtime.load(_owner.game(),
                                       durable_state->scene_id,
                                       carrot::scene::make_scene_load_options(_owner.make_scene_runtime_bindings(),
                                                                              durable_state->spawn_marker))
        };
        _owner._applying_loaded_runtime_state = false;
        if (!loaded)
        {
            status.detail = std::format("Sandbox gameplay durable state failed to load scene '{}' at spawn '{}'.",
                                        durable_state->scene_id,
                                        durable_state->spawn_marker);
            return false;
        }

        status.detail = "Applied sandbox gameplay durable state.";
        return true;
    }
} // namespace sandbox
