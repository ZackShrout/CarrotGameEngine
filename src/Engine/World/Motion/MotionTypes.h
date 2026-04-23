#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::world {
    struct movement_intent_t
    {
        chlm::float2 move_direction{ 0.f, 0.f };
    };

    struct movement_step_t
    {
        movement_intent_t intent{ };
        float max_speed{ 4.0f };
        float delta_time{ 0.f };
    };

    struct movement_result_t
    {
        chlm::float2 requested_direction{ 0.f, 0.f };
        chlm::float2 actual_direction{ 0.f, 0.f };
        chlm::float2 requested_delta{ 0.f, 0.f };
        chlm::float2 actual_delta{ 0.f, 0.f };
        bool blocked_x{ false };
        bool blocked_y{ false };
        bool started_overlapping{ false };

        [[nodiscard]] bool moved() const noexcept
        {
            return actual_delta.x != 0.f || actual_delta.y != 0.f;
        }
    };
} // namespace carrot::world
