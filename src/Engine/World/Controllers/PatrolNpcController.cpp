//
// Created by Zack Shrout on 4/23/26.
//

#include "Core/Pch.h"

#include "PatrolNpcController.h"

namespace carrot::world {
    namespace {
        constexpr float k_movement_epsilon{ 1.0e-4f };

        [[nodiscard]] bool has_meaningful_direction(const chlm::float2 movement) noexcept
        {
            return std::fabs(movement.x) > k_movement_epsilon || std::fabs(movement.y) > k_movement_epsilon;
        }

        [[nodiscard]] float vector_length(const chlm::float2 value) noexcept
        {
            return std::sqrt((value.x * value.x) + (value.y * value.y));
        }

        [[nodiscard]] facing_direction_t facing_from_movement(const chlm::float2 movement,
                                                              const facing_direction_t current) noexcept
        {
            if (!has_meaningful_direction(movement))
                return current;

            if (std::fabs(movement.x) >= std::fabs(movement.y))
                return movement.x < 0.f ? facing_direction_t::left : facing_direction_t::right;

            return movement.y < 0.f ? facing_direction_t::up : facing_direction_t::down;
        }

        [[nodiscard]] const std::string& idle_animation_for(const facing_direction_t direction,
                                                            const player_controller_animation_set_t& animation_set) noexcept
        {
            switch (direction)
            {
                case facing_direction_t::up: return animation_set.idle_up;
                case facing_direction_t::left: return animation_set.idle_left;
                case facing_direction_t::right: return animation_set.idle_right;
                case facing_direction_t::down: return animation_set.idle_down;
            }

            return animation_set.idle_down;
        }

        [[nodiscard]] const std::string& walk_animation_for(const facing_direction_t direction,
                                                            const player_controller_animation_set_t& animation_set) noexcept
        {
            switch (direction)
            {
                case facing_direction_t::up: return animation_set.walk_up;
                case facing_direction_t::left: return animation_set.walk_left;
                case facing_direction_t::right: return animation_set.walk_right;
                case facing_direction_t::down: return animation_set.walk_down;
            }

            return animation_set.walk_down;
        }
    } // namespace

    void patrol_npc_controller_t::set_controlled_object(world_object_t* object) noexcept
    {
        _controlled_object = object;
        _movement_body.bind(_controlled_object);
        _facing_direction = facing_direction_t::down;
        _current_animation = _animation_set.idle_down;
    }

    void patrol_npc_controller_t::set_animation_set(player_controller_animation_set_t animation_set)
    {
        _animation_set = std::move(animation_set);
        _current_animation = _animation_set.idle_down;
    }

    void patrol_npc_controller_t::set_route_points(std::vector<chlm::float2> route_points_world)
    {
        _route_points_world = std::move(route_points_world);
        _target_index = 0u;
    }

    void patrol_npc_controller_t::update(world_t& world, const float delta_time)
    {
        _movement_intent = movement_intent_t{ };

        if (!_controlled_object || !_controlled_object->transform || _route_points_world.empty())
        {
            if (_controlled_object)
                apply_animation(*_controlled_object, _facing_direction, false);
            return;
        }

        while (!_route_points_world.empty())
        {
            const chlm::float2 current_position{ _controlled_object->transform->position };
            const chlm::float2 target{ _route_points_world[_target_index] };
            const chlm::float2 to_target{ target - current_position };
            if (vector_length(to_target) > _waypoint_reached_distance)
                break;

            _target_index = (_target_index + 1u) % _route_points_world.size();
            if (_route_points_world.size() == 1u)
                break;
        }

        if (!_route_points_world.empty())
        {
            const chlm::float2 to_target{
                _route_points_world[_target_index] - _controlled_object->transform->position
            };
            const float distance{ vector_length(to_target) };
            if (distance > _waypoint_reached_distance && distance > k_movement_epsilon)
                _movement_intent.move_direction = to_target / distance;
        }

        _last_move_result = _movement_motor.update(world,
                                                   _movement_body,
                                                   movement_step_t{
                                                       .intent = _movement_intent,
                                                       .max_speed = _move_speed,
                                                       .delta_time = delta_time
                                                   });
        _facing_direction = facing_from_movement(_last_move_result.requested_direction, _facing_direction);
        apply_animation(*_controlled_object, _facing_direction, _last_move_result.moved());
    }

    void patrol_npc_controller_t::apply_animation(world_object_t& object,
                                                  const facing_direction_t facing,
                                                  const bool moving)
    {
        if (!object.sprite_animator || !object.sprite)
            return;

        auto& animator{ object.sprite_animator->animator };
        const assets::loaded_sprite_asset_t* sprite{ object.sprite->sprite };
        if (animator.sprite() != sprite && sprite)
        {
            animator.set_sprite(sprite);
            _current_animation.clear();
        }

        if (!sprite)
            return;

        const std::string& desired_walk_animation{ walk_animation_for(facing, _animation_set) };
        const std::string& desired_idle_animation{ idle_animation_for(facing, _animation_set) };
        const std::string* desired_animation{ &desired_idle_animation };
        const std::string& fallback_idle_animation{ _animation_set.idle_down };

        if (moving && sprite->find_animation(desired_walk_animation))
            desired_animation = &desired_walk_animation;

        if (!sprite->find_animation(*desired_animation) && *desired_animation != fallback_idle_animation)
            desired_animation = &fallback_idle_animation;

        if (_current_animation != *desired_animation)
        {
            animator.play(*desired_animation);
            _current_animation = *desired_animation;
        }
    }
} // namespace carrot::world
