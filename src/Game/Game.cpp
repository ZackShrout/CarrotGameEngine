//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Game.h"

#include "SandboxSceneBootstrap.h"
#include "SceneHelpers.h"

namespace sandbox {
    namespace {
        carrot::audio::voice_handle_t handle;
        constexpr std::string_view background_music_asset_id{ "music.oak_battle_theme" };
    }

    void sandbox_t::start(carrot::core::game_context_t& game)
    {
        _game = &game;
        bootstrap_scene(game);
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
        _player_controller.set_controlled_object(find_player(game.world));
        _player_controller.set_move_speed(4.0f);
        _player_controller.set_camera_follow_enabled(true);
        _interaction_controller.set_actor(_player_controller.controlled_object());
        _interaction_controller.set_interaction_radius(3.0f);
        handle = carrot::audio::play(background_music_asset_id);
    }

    void sandbox_t::on_tick(const float delta_time)
    {
        if (!_game)
            return;

        _player_controller.update(*_game, delta_time);
    }

    void sandbox_t::on_key(const carrot::events::key_event_t& e)
    {
        ce_application_t::on_key(e);

        if (e._action == carrot::events::key_action::press)
        {
            if (e._key == carrot::input::key_code::e && _game)
            {
                LOG_CORE_INFO("Key pressed: {} ({}) (mods: {})", carrot::input::key_code_to_string(e._key),
                              static_cast<uint32_t>(e._key), carrot::input::modifiers_to_string(e._mods));
                if (!_interaction_controller.actor() || !_interaction_controller.actor()->transform)
                {
                    LOG_CORE_WARN("Interaction failed: controlled player world object is missing a transform");
                }
                else if (_interaction_controller.try_interact(*_game))
                {
                }
                else
                {
                    LOG_CORE_INFO("No interactable in range");
                }
            }
        }
        else if (e._action == carrot::events::key_action::repeat &&
                 (e._key == carrot::input::key_code::e ||
                  e._key == carrot::input::key_code::escape ||
                  e._key == carrot::input::key_code::enter ||
                  e._key == carrot::input::key_code::f11))
            LOG_CORE_INFO("Key held: {} ({})", carrot::input::key_code_to_string(e._key),
                      static_cast<uint32_t>(e._key));
        else if (e._action == carrot::events::key_action::release &&
                 (e._key == carrot::input::key_code::e ||
                  e._key == carrot::input::key_code::escape ||
                  e._key == carrot::input::key_code::enter ||
                  e._key == carrot::input::key_code::f11))
            LOG_CORE_INFO("Key released: {} ({})", carrot::input::key_code_to_string(e._key),
                      static_cast<uint32_t>(e._key));

        const bool is_pressed{ e._action == carrot::events::key_action::press };
        const bool is_released{ e._action == carrot::events::key_action::release };
        if (is_pressed || is_released)
        {
            const bool value{ is_pressed };

            if (e._key == carrot::input::key_code::w)
                _player_controller.set_move_up(value);
            else if (e._key == carrot::input::key_code::s)
                _player_controller.set_move_down(value);
            else if (e._key == carrot::input::key_code::a)
                _player_controller.set_move_left(value);
            else if (e._key == carrot::input::key_code::d)
                _player_controller.set_move_right(value);
        }

        if (e._action == carrot::events::key_action::press && e._key == carrot::input::key_code::escape)
            quit_application();

        if (e._action == carrot::events::key_action::press && (
                (e._key == carrot::input::key_code::enter && carrot::input::has_modifier(
                     e._mods, carrot::input::modifier::alt)) || e._key == carrot::input::key_code::f11))
        {
            set_fullscreen(!is_fullscreen());
        }
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
