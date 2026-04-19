//
// Created by Zack Shrout on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/AssetManager.h"
#include "Core/GameContext.h"
#include "Core/GameView.h"
#include "EngineConfig.h"
#include "Input/ActionMap.h"
#include "Input/ControllerManager.h"
#include "Input/GameplayInputRouter.h"
#include "Renderer/Renderer.h"
#include "RHI/RHI.h"
#include "Window/Window.h"
#include "World/Controllers/InteractionController.h"
#include "World/Controllers/PlayerController.h"
#include "World/World.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        [[nodiscard]] std::unique_ptr<rhi::rhi_context_t> make_null_rhi()
        {
            return rhi::create_rhi_context(rhi::rhi_desc_t{
                .api = rhi::graphics_api::null_backend,
                .presentation_window_id = window::invalid_window_id,
                .width = 1280u,
                .height = 720u,
                .enable_debug_layers = false
            });
        }

        void test_controller_manager_default_snapshot_has_no_active_gamepad()
        {
            input::controller_manager_t manager;
            const input::controller_debug_snapshot_t snapshot{ manager.debug_snapshot() };

            CARROT_TEST_REQUIRE(snapshot.connected_gamepad_count == 0u);
            CARROT_TEST_REQUIRE(!snapshot.active_gamepad_index.has_value());
            CARROT_TEST_REQUIRE(!snapshot.active_gamepad.connected);
        }

        void test_action_map_keeps_action_pressed_while_gamepad_binding_remains_active()
        {
            input::input_action_map_t actions;
            actions.bind("interact", input::key_code::e);
            actions.bind_gamepad_button("interact", input::gamepad_button_t::south);

            const events::key_event_t key_press{
                ._key = input::key_code::e,
                ._action = events::key_action::press
            };
            actions.handle_key_event(key_press);
            CARROT_TEST_REQUIRE(actions.is_pressed("interact"));

            input::gamepad_state_t gamepad{ .connected = true };
            gamepad.buttons[static_cast<size_t>(input::gamepad_button_t::south)] = true;
            actions.update_gamepad_state(&gamepad);
            CARROT_TEST_REQUIRE(actions.is_pressed("interact"));

            const events::key_event_t key_release{
                ._key = input::key_code::e,
                ._action = events::key_action::release
            };
            actions.handle_key_event(key_release);
            CARROT_TEST_REQUIRE(actions.is_pressed("interact"));

            actions.update_gamepad_state(nullptr);
            CARROT_TEST_REQUIRE(!actions.is_pressed("interact"));
        }

        void test_player_controller_uses_move_intent_when_present()
        {
            world::world_t world;
            world::world_object_t player;
            player.transform = world::transform_component_t{
                .position = { 0.f, 0.f },
                .scale = { 1.f, 1.f }
            };

            world::player_controller_t controller;
            controller.set_controlled_object(&player);
            controller.set_move_speed(1.f);
            controller.set_move_input(true, false, false, false);
            controller.set_move_intent({ 3.f, 4.f });

            const world::player_move_result_t result{ controller.update(world, 1.f) };

            CARROT_TEST_REQUIRE(std::fabs(result.actual_delta.x - 0.6f) < 1.0e-4f);
            CARROT_TEST_REQUIRE(std::fabs(result.actual_delta.y - 0.8f) < 1.0e-4f);
        }

        void test_player_controller_falls_back_to_boolean_move_input_without_move_intent()
        {
            world::world_t world;
            world::world_object_t player;
            player.transform = world::transform_component_t{
                .position = { 0.f, 0.f },
                .scale = { 1.f, 1.f }
            };

            world::player_controller_t controller;
            controller.set_controlled_object(&player);
            controller.set_move_speed(1.f);
            controller.set_move_input(true, false, false, false);
            controller.set_move_intent({ 0.f, 0.f });

            const world::player_move_result_t result{ controller.update(world, 1.f) };

            CARROT_TEST_REQUIRE(std::fabs(result.actual_delta.x) < 1.0e-4f);
            CARROT_TEST_REQUIRE(std::fabs(result.actual_delta.y + 1.f) < 1.0e-4f);
        }

        void test_player_controller_clear_movement_input_resets_intent_and_digital_state()
        {
            world::player_controller_t controller;
            controller.set_move_input(true, false, true, false);
            controller.set_move_intent({ 1.f, -1.f });

            controller.clear_movement_input();

            const chlm::float2 intent{ controller.move_intent() };
            CARROT_TEST_REQUIRE(intent.x == 0.f);
            CARROT_TEST_REQUIRE(intent.y == 0.f);
        }

        void test_interaction_controller_attempt_result_reports_missing_actor_and_candidate()
        {
            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer{ vfs, graphics_config, window::invalid_window_id };
            assets::asset_manager_t assets{ vfs, *renderer.get_rhi() };
            world::world_t world;
            core::game_view_t view{ renderer };
            input::controller_manager_t controllers;
            core::game_context_t game{
                .world = world,
                .assets = assets,
                .view = view,
                .controllers = controllers
            };

            world::interaction_controller_t controller;
            CARROT_TEST_REQUIRE(controller.attempt_interaction(game) == world::interaction_attempt_result_t::no_actor);

            world::world_object_t actor;
            controller.set_actor(&actor);
            CARROT_TEST_REQUIRE(controller.has_actor());
            CARROT_TEST_REQUIRE(controller.attempt_interaction(game) == world::interaction_attempt_result_t::actor_missing_transform);

            actor.transform = world::transform_component_t{
                .position = { 0.f, 0.f },
                .scale = { 1.f, 1.f }
            };
            CARROT_TEST_REQUIRE(controller.attempt_interaction(game) == world::interaction_attempt_result_t::no_candidate);
        }

        void test_gameplay_input_router_fixed_multiplayer_helper_assigns_expected_defaults()
        {
            const input::gameplay_input_routing_config_t config{
                input::make_fixed_local_multiplayer_routing_config(2u)
            };

            CARROT_TEST_REQUIRE(config.mode == input::gameplay_input_routing_mode_t::local_multiplayer_fixed);
            CARROT_TEST_REQUIRE(config.player_count == 2u);
            CARROT_TEST_REQUIRE(config.assignments[0].receives_keyboard);
            CARROT_TEST_REQUIRE(config.assignments[0].gamepad_slot.has_value() &&
                                *config.assignments[0].gamepad_slot == 0u);
            CARROT_TEST_REQUIRE(!config.assignments[1].receives_keyboard);
            CARROT_TEST_REQUIRE(config.assignments[1].gamepad_slot.has_value() &&
                                *config.assignments[1].gamepad_slot == 1u);
        }

        void test_gameplay_input_router_single_player_auto_normalizes_to_default_context()
        {
            input::gameplay_input_router_t router;
            input::gameplay_input_routing_config_t config{
                .mode = input::gameplay_input_routing_mode_t::single_player_auto,
                .player_count = 3u
            };
            config.assignments[0] = input::player_input_assignment_t{
                .receives_keyboard = false,
                .gamepad_slot = 2u
            };

            router.configure_routing(config);

            CARROT_TEST_REQUIRE(router.routing_mode() == input::gameplay_input_routing_mode_t::single_player_auto);
            CARROT_TEST_REQUIRE(router.player_count() == 1u);

            const input::player_input_context_t* player{ router.player(0u) };
            CARROT_TEST_REQUIRE(player);
            CARROT_TEST_REQUIRE(player->receives_keyboard());
            CARROT_TEST_REQUIRE(!player->gamepad_slot().has_value());
            CARROT_TEST_REQUIRE(router.describe_routing() == "mode=single_player_auto, players=1, P1 (keyboard, gamepad=none)");
        }
    } // namespace

    void register_controller_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("controller manager default snapshot has no active gamepad",
                           test_controller_manager_default_snapshot_has_no_active_gamepad);
        tests.emplace_back("action map keeps action pressed while gamepad binding remains active",
                           test_action_map_keeps_action_pressed_while_gamepad_binding_remains_active);
        tests.emplace_back("player controller uses move intent when present",
                           test_player_controller_uses_move_intent_when_present);
        tests.emplace_back("player controller falls back to boolean move input without move intent",
                           test_player_controller_falls_back_to_boolean_move_input_without_move_intent);
        tests.emplace_back("player controller clear movement input resets intent and digital state",
                           test_player_controller_clear_movement_input_resets_intent_and_digital_state);
        tests.emplace_back("interaction controller attempt result reports missing actor and candidate",
                           test_interaction_controller_attempt_result_reports_missing_actor_and_candidate);
        tests.emplace_back("gameplay input router fixed multiplayer helper assigns expected defaults",
                           test_gameplay_input_router_fixed_multiplayer_helper_assigns_expected_defaults);
        tests.emplace_back("gameplay input router single-player auto normalizes to default context",
                           test_gameplay_input_router_single_player_auto_normalizes_to_default_context);
    }
} // namespace carrot::tests
