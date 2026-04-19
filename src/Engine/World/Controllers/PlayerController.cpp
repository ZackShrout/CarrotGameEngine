//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "PlayerController.h"
#include "World/World.h"

namespace carrot::world {
    namespace {
        constexpr float k_movement_epsilon{ 1.0e-4f };
        constexpr float k_penetration_slop{ 1.0e-3f };
        constexpr float k_slide_skin{ 1.0e-2f };

        [[nodiscard]] collision_component_t default_collision_component() noexcept
        {
            return collision_component_t{
                .half_extents = { 0.3f, 0.2f },
                .offset = { 0.f, -0.2f },
                .debug_display = std::nullopt
            };
        }

        [[nodiscard]] bool is_moving(const chlm::float2 movement) noexcept
        {
            return movement.x != 0.f || movement.y != 0.f;
        }

        [[nodiscard]] bool has_meaningful_movement(const chlm::float2 movement) noexcept
        {
            return std::fabs(movement.x) > k_movement_epsilon || std::fabs(movement.y) > k_movement_epsilon;
        }

        [[nodiscard]] float movement_length(const chlm::float2 movement) noexcept
        {
            return std::sqrt((movement.x * movement.x) + (movement.y * movement.y));
        }

        [[nodiscard]] chlm::float2 minimum_separation_vector(const collision::collision_aabb_t& moving,
                                                             const collision::collision_aabb_t& blocking) noexcept
        {
            const float push_left{ blocking.min.x - moving.max.x };
            const float push_right{ blocking.max.x - moving.min.x };
            const float push_up{ blocking.min.y - moving.max.y };
            const float push_down{ blocking.max.y - moving.min.y };

            chlm::float2 best_push{ push_left, 0.f };
            float best_distance{ std::fabs(push_left) };

            const auto consider = [&](const chlm::float2 candidate) {
                const float candidate_distance{
                    std::fabs(candidate.x) + std::fabs(candidate.y)
                };
                if (candidate_distance < best_distance)
                {
                    best_push = candidate;
                    best_distance = candidate_distance;
                }
            };

            consider(chlm::float2{ push_right, 0.f });
            consider(chlm::float2{ 0.f, push_up });
            consider(chlm::float2{ 0.f, push_down });
            return best_push;
        }

        void depenetrate_position_against_world(const world_t& world,
                                                const player_controller_t& controller,
                                                chlm::float2& position) noexcept
        {
            for (uint32_t iteration{ 0 }; iteration < 4u; ++iteration)
            {
                const collision::collision_aabb_t bounds{ controller.collision_bounds_at(position) };
                const std::vector<collision::collision_hit_ref_t> overlaps{
                    world.collision_world().overlap_query(bounds)
                };

                if (overlaps.empty())
                    return;

                chlm::float2 best_push{ 0.f, 0.f };
                float best_distance{ 0.f };
                bool found_push{ false };

                for (const collision::collision_hit_ref_t& overlap : overlaps)
                {
                    const chlm::float2 candidate_push{ minimum_separation_vector(bounds, overlap.bounds) };
                    const float candidate_distance{
                        std::fabs(candidate_push.x) + std::fabs(candidate_push.y)
                    };

                    if (!found_push || candidate_distance < best_distance)
                    {
                        best_push = candidate_push;
                        best_distance = candidate_distance;
                        found_push = true;
                    }
                }

                if (!found_push)
                    return;

                if (best_push.x != 0.f)
                    best_push.x += std::signbit(best_push.x) ? -k_penetration_slop : k_penetration_slop;
                if (best_push.y != 0.f)
                    best_push.y += std::signbit(best_push.y) ? -k_penetration_slop : k_penetration_slop;

                position += best_push;
            }
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

    void player_controller_t::clear_movement_input() noexcept
    {
        _move_up = false;
        _move_down = false;
        _move_left = false;
        _move_right = false;
        _move_intent = { 0.f, 0.f };
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
        if (_controlled_object && !_controlled_object->collision)
            _controlled_object->collision = default_collision_component();
        _facing_direction = facing_direction_t::down;
        _current_animation = _animation_set.idle_down;
    }

    void player_controller_t::update([[maybe_unused]] core::game_context_t& game, const float delta_time)
    {
        _last_move_result = update(game.world, delta_time);
    }

    player_move_result_t player_controller_t::update(world_t& world, const float delta_time)
    {
        if (!_controlled_object || !_controlled_object->transform)
            return player_move_result_t{ };

        chlm::float2 movement{ 0.f, 0.f };
        if (std::fabs(_move_intent.x) > k_movement_epsilon || std::fabs(_move_intent.y) > k_movement_epsilon)
        {
            movement = _move_intent;
        }
        else
        {
            if (_move_up)
                movement.y -= 1.f;
            if (_move_down)
                movement.y += 1.f;
            if (_move_left)
                movement.x -= 1.f;
            if (_move_right)
                movement.x += 1.f;
        }

        const float length_sq{ (movement.x * movement.x) + (movement.y * movement.y) };
        const bool moving{ length_sq > 0.f };
        if (moving)
        {
            const float length{ std::sqrt(length_sq) };
            if (length > 1.f)
            {
                movement.x /= length;
                movement.y /= length;
            }
        }

        _last_move_result = move(world, movement * _move_speed * delta_time);
        _facing_direction = facing_from_movement(movement, _facing_direction);
        apply_animation(*_controlled_object, _facing_direction, moving);
        return _last_move_result;
    }

    player_move_result_t player_controller_t::move(world_t& world, const chlm::float2 delta)
    {
        player_move_result_t result{ .requested_delta = delta };
        if (!_controlled_object || !_controlled_object->transform || !has_meaningful_movement(delta))
            return result;

        chlm::float2 remaining_delta{ delta };
        chlm::float2 current_position{ _controlled_object->transform->position };
        const size_t overlap_count_before_move{
            world.collision_world().overlap_query(collision_bounds_at(current_position)).size()
        };
        if (overlap_count_before_move > 0u)
        {
            result.started_overlapping = true;
            depenetrate_position_against_world(world, *this, current_position);
        }

        for (uint32_t iteration{ 0 }; iteration < 2u; ++iteration)
        {
            if (!has_meaningful_movement(remaining_delta))
                break;

            const std::optional<collision::sweep_hit_t> hit{
                world.collision_world().sweep_aabb(collision_bounds_at(current_position), remaining_delta)
            };
            if (!hit)
            {
                current_position += remaining_delta;
                result.actual_delta += remaining_delta;
                remaining_delta = { 0.f, 0.f };
                break;
            }

            if (hit->started_overlapping)
            {
                result.started_overlapping = true;
                result.blocked_x = result.blocked_x || std::fabs(remaining_delta.x) > k_movement_epsilon;
                result.blocked_y = result.blocked_y || std::fabs(remaining_delta.y) > k_movement_epsilon;
                remaining_delta = { 0.f, 0.f };
                break;
            }

            const float remaining_length{ movement_length(remaining_delta) };
            float safe_fraction{ hit->fraction };
            if (remaining_length > k_movement_epsilon)
                safe_fraction = std::max(0.f, hit->fraction - (k_slide_skin / remaining_length));

            const chlm::float2 travelled_delta{ remaining_delta * safe_fraction };
            current_position += travelled_delta;
            result.actual_delta += travelled_delta;
            const chlm::float2 desired_remaining_delta{ remaining_delta * (1.f - safe_fraction) };
            remaining_delta = desired_remaining_delta;

            if (hit->normal.x != 0.f)
                result.blocked_x = true;

            if (hit->normal.y != 0.f)
                result.blocked_y = true;

            const float into_surface{
                (remaining_delta.x * hit->normal.x) + (remaining_delta.y * hit->normal.y)
            };
            if (into_surface < 0.f)
            {
                remaining_delta.x -= hit->normal.x * into_surface;
                remaining_delta.y -= hit->normal.y * into_surface;
            }
        }

        _controlled_object->transform->position = current_position;
        return result;
    }

    collision::collision_aabb_t player_controller_t::collision_bounds_at(const chlm::float2 position) const noexcept
    {
        const collision_component_t collision{
            _controlled_object && _controlled_object->collision ? *_controlled_object->collision : default_collision_component()
        };
        return collision::collision_aabb_t::from_center_extents(position + collision.offset, collision.half_extents);
    }

    collision::collision_aabb_t player_controller_t::current_collision_bounds() const noexcept
    {
        if (!_controlled_object || !_controlled_object->transform)
            return collision::collision_aabb_t{ };

        return collision_bounds_at(_controlled_object->transform->position);
    }

    void player_controller_t::apply_animation(world_object_t& object,
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
