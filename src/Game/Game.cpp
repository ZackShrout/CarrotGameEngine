//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Game.h"

#include "Assets/Scene/SceneAsset.h"
#include "SandboxSceneBootstrap.h"
#include "SceneHelpers.h"

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
    }

    void sandbox_t::configure_default_input_actions()
    {
        _actions.clear();

        _actions.bind(std::string{ k_action_move_up }, carrot::input::key_code::w);
        _actions.bind(std::string{ k_action_move_up }, carrot::input::key_code::up);
        _actions.bind(std::string{ k_action_move_down }, carrot::input::key_code::s);
        _actions.bind(std::string{ k_action_move_down }, carrot::input::key_code::down);
        _actions.bind(std::string{ k_action_move_left }, carrot::input::key_code::a);
        _actions.bind(std::string{ k_action_move_left }, carrot::input::key_code::left);
        _actions.bind(std::string{ k_action_move_right }, carrot::input::key_code::d);
        _actions.bind(std::string{ k_action_move_right }, carrot::input::key_code::right);

        _actions.bind(std::string{ k_action_interact }, carrot::input::key_code::e);
        _actions.bind(std::string{ k_action_quit }, carrot::input::key_code::escape);
        _actions.bind(std::string{ k_action_toggle_fullscreen }, carrot::input::key_code::f11);
        _actions.bind(std::string{ k_action_toggle_fullscreen },
                      carrot::input::key_code::enter,
                      static_cast<uint8_t>(carrot::input::modifier::alt));
    }

    void sandbox_t::refresh_scene_bindings() noexcept
    {
        if (!_game)
            return;

        _player_controller.set_controlled_object(find_player(_game->world));
        _interaction_controller.set_actor(_player_controller.controlled_object());

        if (_player_controller.controlled_object() && _player_controller.controlled_object()->transform)
        {
            _game->view.set_center_world_position(_game->world, _player_controller.controlled_object()->transform->position);
        }
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

    bool sandbox_t::load_scene(const std::string_view scene_id, const std::string_view spawn_marker)
    {
        if (!_game)
            return false;

        if (!bootstrap_scene(*_game, scene_id, spawn_marker))
            return false;

        _current_scene_id = std::string{ scene_id };
        refresh_scene_bindings();
        refresh_scene_music();
        return true;
    }

    void sandbox_t::start(carrot::core::game_context_t& game)
    {
        _game = &game;
        configure_default_input_actions();
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
        _player_controller.set_camera_follow_enabled(true);
        _interaction_controller.set_interaction_radius(3.0f);
        load_scene(k_bootstrap_scene_id);
    }

    void sandbox_t::on_tick(const float delta_time)
    {
        if (!_game)
            return;

        if (const std::optional<scene_transition_request_t> request{ _interaction_controller.consume_pending_transition() })
            load_scene(request->scene_id, request->marker_name);

        _player_controller.update(*_game, delta_time);
    }

    void sandbox_t::on_key(const carrot::events::key_event_t& e)
    {
        ce_application_t::on_key(e);
        _actions.handle_key_event(e);

        if (e._action == carrot::events::key_action::press)
        {
            if (_actions.matches(k_action_interact, e) && _game)
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

        if (e._action == carrot::events::key_action::press || e._action == carrot::events::key_action::release)
        {
            _player_controller.set_move_up(_actions.is_pressed(k_action_move_up));
            _player_controller.set_move_down(_actions.is_pressed(k_action_move_down));
            _player_controller.set_move_left(_actions.is_pressed(k_action_move_left));
            _player_controller.set_move_right(_actions.is_pressed(k_action_move_right));
        }

        if (e._action == carrot::events::key_action::press && _actions.matches(k_action_quit, e))
            quit_application();

        if (e._action == carrot::events::key_action::press && _actions.matches(k_action_toggle_fullscreen, e))
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
