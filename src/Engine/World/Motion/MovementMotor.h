#pragma once

#include "MotionTypes.h"
#include "MovementBody.h"

namespace carrot::world {
    class world_t;

    class movement_motor_t
    {
    public:
        virtual ~movement_motor_t() = default;

        [[nodiscard]] virtual movement_result_t update(world_t& world,
                                                       movement_body_t& body,
                                                       const movement_step_t& step) const = 0;
        [[nodiscard]] virtual movement_result_t move(world_t& world,
                                                     movement_body_t& body,
                                                     chlm::float2 requested_delta) const = 0;
    };
} // namespace carrot::world
