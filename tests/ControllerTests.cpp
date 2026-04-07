//
// Created by Zack Shrout on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Input/ActionMap.h"
#include "Input/ControllerManager.h"
#include "World/Controllers/PlayerController.h"
#include "World/World.h"

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
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
    }
} // namespace carrot::tests
