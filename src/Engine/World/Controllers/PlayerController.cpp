//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "PlayerController.h"

namespace carrot::world {
    namespace {
        [[nodiscard]] bool is_moving(const chlm::float2 movement) noexcept
        {
            return movement.x != 0.f || movement.y != 0.f;
        }

        [[nodiscard]] facing_direction_t facing_from_movement(const chlm::float2 movement,
                                                              const facing_direction_t current) noexcept
        {
            if (!is_moving(movement))
                return current;

            if (std::fabs(movement.x) >= std::fabs(movement.y))
                return movement.x < 0.f ? facing_direction_t::left : facing_direction_t::right;

            return movement.y < 0.f ? facing_direction_t::up : facing_direction_t::down;
        }

        [[nodiscard]] std::string_view idle_animation_for(const facing_direction_t direction,
                                                          const player_controller_animation_set_t& animation_set) noexcept
        {
            switch (direction)
            {
                case facing_direction_t::up: return animation_set.idle_up;
                case facing_direction_t::left: return animation_set.idle_left;
                case facing_direction_t::right: return animation_set.idle_right;
                case facing_direction_t::down:
                default: return animation_set.idle_down;
            }
        }

        [[nodiscard]] std::string_view walk_animation_for(const facing_direction_t direction,
                                                          const player_controller_animation_set_t& animation_set) noexcept
        {
            switch (direction)
            {
                case facing_direction_t::up: return animation_set.walk_up;
                case facing_direction_t::left: return animation_set.walk_left;
                case facing_direction_t::right: return animation_set.walk_right;
                case facing_direction_t::down:
                default: return animation_set.walk_down;
            }
        }
    } // namespace

    void player_controller_t::set_move_input(const bool up,
                                             const bool down,
                                             const bool left,
                                             const bool right) noexcept
    {
        _move_up = up;
        _move_down = down;
        _move_left = left;
        _move_right = right;
    }

    void player_controller_t::set_animation_set(player_controller_animation_set_t animation_set)
    {
        _animation_set = std::move(animation_set);
        _current_animation = _animation_set.idle_down;
    }

    void player_controller_t::set_facing_direction(const facing_direction_t direction)
    {
        _facing_direction = direction;

        if (_controlled_object)
            apply_animation(*_controlled_object, _facing_direction, false);
    }

    void player_controller_t::set_controlled_object(world_object_t* object) noexcept
    {
        _controlled_object = object;
        _facing_direction = facing_direction_t::down;
        _current_animation = _animation_set.idle_down;
    }

    void player_controller_t::update([[maybe_unused]] core::game_context_t& game, const float delta_time)
    {
        if (!_controlled_object || !_controlled_object->transform)
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

        const bool moving{ is_moving(movement) };
        if (moving)
        {
            const float length_sq{ (movement.x * movement.x) + (movement.y * movement.y) };
            if (length_sq > 0.f)
            {
                const float length{ std::sqrt(length_sq) };
                movement.x /= length;
                movement.y /= length;
            }

            _controlled_object->transform->position.x += movement.x * _move_speed * delta_time;
            _controlled_object->transform->position.y += movement.y * _move_speed * delta_time;
        }

        _facing_direction = facing_from_movement(movement, _facing_direction);
        apply_animation(*_controlled_object, _facing_direction, moving);
    }

    void player_controller_t::apply_animation(world_object_t& object,
                                              const facing_direction_t facing,
                                              const bool moving)
    {
        if (!object.sprite_animator || !object.sprite)
            return;

        auto& animator{ object.sprite_animator->animator };
        const assets::loaded_sprite_asset_t* sprite{ object.sprite->sprite };
        if (!animator.sprite() && sprite)
            animator.set_sprite(sprite);

        const std::string_view desired_walk_animation{ walk_animation_for(facing, _animation_set) };
        const std::string_view desired_idle_animation{ idle_animation_for(facing, _animation_set) };
        std::string_view desired_animation{ desired_idle_animation };
        const std::string_view fallback_idle_animation{ _animation_set.idle_down };

        if (moving && sprite && sprite->find_animation(desired_walk_animation))
            desired_animation = desired_walk_animation;

        if ((!sprite || !sprite->find_animation(desired_animation)) && desired_animation != fallback_idle_animation)
            desired_animation = fallback_idle_animation;

        if (_current_animation != desired_animation)
        {
            animator.play(desired_animation);
            _current_animation = std::string{ desired_animation };
        }
    }
} // namespace carrot::world
