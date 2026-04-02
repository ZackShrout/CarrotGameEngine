//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Game.h"
#include "SandboxSceneBootstrap.h"

namespace sandbox {
    namespace {
        carrot::audio::voice_handle_t handle;
        constexpr std::string_view background_music_asset_id{ "music.oak_battle_theme" };
        constexpr float sign_interaction_radius{ 3.0f };

        [[nodiscard]] float distance_sq(const chlm::float2 a, const chlm::float2 b) noexcept
        {
            const float dx{ a.x - b.x };
            const float dy{ a.y - b.y };
            return (dx * dx) + (dy * dy);
        }

        [[nodiscard]] const carrot::world::world_object_t* find_nearest_supported_interactable(
            carrot::world::world_t& world,
            const chlm::float2& origin,
            const float max_distance)
        {
            const carrot::world::world_object_t* nearest{ nullptr };
            float nearest_distance_sq{ max_distance * max_distance };

            for (const std::string_view type : { std::string_view{ "Sign" }, std::string_view{ "Door" }, std::string_view{ "Chest" } })
            {
                const carrot::world::world_object_t* candidate{
                    world.find_nearest_object_by_type(type, origin, max_distance)
                };

                if (!candidate || !candidate->transform)
                    continue;

                if (!candidate->get_bool_property("interactable").value_or(false))
                    continue;

                const float candidate_distance_sq{ distance_sq(candidate->transform->position, origin) };
                if (!nearest || candidate_distance_sq < nearest_distance_sq)
                {
                    nearest = candidate;
                    nearest_distance_sq = candidate_distance_sq;
                }
            }

            return nearest;
        }

        [[nodiscard]] std::string_view idle_animation_for(const facing_direction_t direction) noexcept
        {
            switch (direction)
            {
                case facing_direction_t::up: return "idle_up";
                case facing_direction_t::left: return "idle_left";
                case facing_direction_t::right: return "idle_right";
                case facing_direction_t::down:
                default: return "idle_down";
            }
        }

        [[nodiscard]] std::string_view walk_animation_for(const facing_direction_t direction) noexcept
        {
            switch (direction)
            {
                case facing_direction_t::up: return "walk_up";
                case facing_direction_t::left: return "walk_left";
                case facing_direction_t::right: return "walk_right";
                case facing_direction_t::down:
                default: return "walk_down";
            }
        }

        [[nodiscard]] facing_direction_t facing_from_movement(const chlm::float2 movement,
                                                              const facing_direction_t current) noexcept
        {
            if (movement.x == 0.f && movement.y == 0.f)
                return current;

            if (std::fabs(movement.x) > std::fabs(movement.y))
                return movement.x < 0.f ? facing_direction_t::left : facing_direction_t::right;

            return movement.y < 0.f ? facing_direction_t::up : facing_direction_t::down;
        }

        void log_interaction_target(const carrot::world::world_object_t& object)
        {
            if (object.type == "Sign")
            {
                LOG_CORE_INFO("Interact Sign '{}' -> message_id='{}'",
                              object.name,
                              object.get_string_property("message_id").value_or("<missing>"));
                return;
            }

            if (object.type == "Door")
            {
                LOG_CORE_INFO("Interact Door '{}' -> target_map='{}', target_marker='{}'",
                              object.name,
                              object.get_string_property("target_map").value_or("<missing>"),
                              object.get_string_property("target_marker").value_or("<missing>"));
                return;
            }

            if (object.type == "Chest")
            {
                LOG_CORE_INFO("Interact Chest '{}' -> loot_table='{}'",
                              object.name,
                              object.get_string_property("loot_table").value_or("<missing>"));
                return;
            }

            LOG_CORE_INFO("Interact '{}' of type '{}'", object.name, object.type);
        }
    }

    void sandbox_t::start(carrot::engine_t& engine)
    {
        _engine = &engine;
        bootstrap_scene(engine);
        handle = carrot::audio::play(background_music_asset_id);
    }

    void sandbox_t::on_tick(const float delta_time)
    {
        if (!_engine)
            return;

        carrot::world::world_t& world{ _engine->world() };
        carrot::world::world_object_t* player{ world.find_object_by_name("Vraden") };
        if (!player || !player->transform)
            return;

        chlm::float2 movement{ 0.f, 0.f };
        if (_move_up)
            movement.y -= 1.f;
        if (_move_down)
            movement.y += 1.f;
        if (_move_left)
            movement.x -= 1.f;
        if (_move_right)
            movement.x += 1.f;

        if (movement.x != 0.f || movement.y != 0.f)
        {
            const float length_sq{ (movement.x * movement.x) + (movement.y * movement.y) };
            if (length_sq > 0.f)
            {
                const float length{ std::sqrt(length_sq) };
                movement.x /= length;
                movement.y /= length;
            }

            player->transform->position.x += movement.x * _player_move_speed * delta_time;
            player->transform->position.y += movement.y * _player_move_speed * delta_time;
        }

        _facing_direction = facing_from_movement(movement, _facing_direction);

        if (player->sprite_animator && player->sprite)
        {
            auto& animator{ player->sprite_animator->animator };
            const carrot::assets::loaded_sprite_asset_t* sprite{ player->sprite->sprite };

            const std::string_view desired_walk_animation{ walk_animation_for(_facing_direction) };
            const std::string_view desired_idle_animation{ idle_animation_for(_facing_direction) };
            std::string_view desired_animation{ desired_idle_animation };

            if (movement.x != 0.f || movement.y != 0.f)
            {
                if (sprite && sprite->find_animation(desired_walk_animation))
                    desired_animation = desired_walk_animation;
            }

            if ((!sprite || !sprite->find_animation(desired_animation)) && desired_animation != "idle_down")
                desired_animation = "idle_down";

            if (_current_player_animation != desired_animation)
            {
                animator.play(desired_animation);
                _current_player_animation = std::string{ desired_animation };
            }
        }

        carrot::renderer::renderer_t& renderer{ _engine->renderer() };
        carrot::renderer::camera_2d_t camera{ renderer.get_camera_2d() };
        const chlm::float2 player_render_position{
            world.presentation().world_position_to_pixels(player->transform->position)
        };
        const chlm::float2 visible_world_size{ renderer.resolve_camera_2d().visible_world_size };
        camera.position = {
            player_render_position.x - (visible_world_size.x * 0.5f),
            player_render_position.y - (visible_world_size.y * 0.5f)
        };
        renderer.set_camera_2d(camera);
    }

    void sandbox_t::on_key(const carrot::events::key_event_t& e)
    {
        ce_application_t::on_key(e);

        if (e._action == carrot::events::key_action::press)
        {
            if (e._key == carrot::input::key_code::e && _engine)
            {
                LOG_CORE_INFO("Key pressed: {} ({}) (mods: {})", carrot::input::key_code_to_string(e._key),
                              static_cast<uint32_t>(e._key), carrot::input::modifiers_to_string(e._mods));
                carrot::world::world_t& world{ _engine->world() };
                const carrot::world::world_object_t* player{ world.find_object_by_name("Vraden") };
                if (!player || !player->transform)
                {
                    LOG_CORE_WARN("Interaction failed: player world object 'Vraden' is missing a transform");
                }
                else if (const carrot::world::world_object_t* interactable{
                    find_nearest_supported_interactable(world, player->transform->position, sign_interaction_radius)
                })
                {
                    log_interaction_target(*interactable);
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
                _move_up = value;
            else if (e._key == carrot::input::key_code::s)
                _move_down = value;
            else if (e._key == carrot::input::key_code::a)
                _move_left = value;
            else if (e._key == carrot::input::key_code::d)
                _move_right = value;
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
