//
// Created by Zack Shrout on 4/23/26.
//

#include "Core/Pch.h"

#include "TopDownMovementMotor.h"

#include "World/World.h"

namespace carrot::world {
    namespace {
        constexpr float k_movement_epsilon{ 1.0e-4f };
        constexpr float k_penetration_slop{ 1.0e-3f };
        constexpr float k_slide_skin{ 1.0e-2f };

        [[nodiscard]] bool has_meaningful_movement(const chlm::float2 movement) noexcept
        {
            return std::fabs(movement.x) > k_movement_epsilon || std::fabs(movement.y) > k_movement_epsilon;
        }

        [[nodiscard]] float movement_length(const chlm::float2 movement) noexcept
        {
            return std::sqrt((movement.x * movement.x) + (movement.y * movement.y));
        }

        [[nodiscard]] chlm::float2 normalize_if_needed(chlm::float2 movement) noexcept
        {
            const float length_sq{ (movement.x * movement.x) + (movement.y * movement.y) };
            if (length_sq <= 0.f)
                return movement;

            const float length{ std::sqrt(length_sq) };
            if (length > 1.f)
            {
                movement.x /= length;
                movement.y /= length;
            }

            return movement;
        }

        [[nodiscard]] chlm::float2 direction_from_delta(const chlm::float2 delta) noexcept
        {
            if (!has_meaningful_movement(delta))
                return { 0.f, 0.f };

            const float length{ movement_length(delta) };
            if (length <= k_movement_epsilon)
                return { 0.f, 0.f };

            return delta / length;
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
                const float candidate_distance{ std::fabs(candidate.x) + std::fabs(candidate.y) };
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
                                                const movement_body_t& body,
                                                chlm::float2& position) noexcept
        {
            for (uint32_t iteration{ 0 }; iteration < 4u; ++iteration)
            {
                const collision::collision_aabb_t bounds{ body.collision_bounds_at(position) };
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
                    const float candidate_distance{ std::fabs(candidate_push.x) + std::fabs(candidate_push.y) };

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
    } // namespace

    movement_result_t top_down_movement_motor_t::update(world_t& world,
                                                        movement_body_t& body,
                                                        const movement_step_t& step) const
    {
        const chlm::float2 move_direction{ normalize_if_needed(step.intent.move_direction) };
        movement_result_t result{
            .requested_direction = move_direction,
            .requested_delta = move_direction * step.max_speed * step.delta_time
        };

        if (!body.has_object() || !body.has_transform())
            return result;

        movement_result_t move_result{ move(world, body, result.requested_delta) };
        move_result.requested_direction = move_direction;
        return move_result;
    }

    movement_result_t top_down_movement_motor_t::move(world_t& world,
                                                      movement_body_t& body,
                                                      const chlm::float2 requested_delta) const
    {
        movement_result_t result{
            .requested_direction = direction_from_delta(requested_delta),
            .requested_delta = requested_delta
        };
        if (!body.has_object() || !body.has_transform() || !has_meaningful_movement(requested_delta))
            return result;

        chlm::float2 remaining_delta{ requested_delta };
        chlm::float2 current_position{ body.bound_object()->transform->position };
        const size_t overlap_count_before_move{
            world.collision_world().overlap_query(body.collision_bounds_at(current_position)).size()
        };
        if (overlap_count_before_move > 0u)
        {
            result.started_overlapping = true;
            depenetrate_position_against_world(world, body, current_position);
        }

        for (uint32_t iteration{ 0 }; iteration < 2u; ++iteration)
        {
            if (!has_meaningful_movement(remaining_delta))
                break;

            const std::optional<collision::sweep_hit_t> hit{
                world.collision_world().sweep_aabb(body.collision_bounds_at(current_position), remaining_delta)
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
            remaining_delta *= (1.f - safe_fraction);

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

        body.bound_object()->transform->position = current_position;
        result.actual_direction = direction_from_delta(result.actual_delta);
        return result;
    }
} // namespace carrot::world
