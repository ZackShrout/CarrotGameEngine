#pragma once

#include "MovementMotor.h"

namespace carrot::world {
    class top_down_movement_motor_t final : public movement_motor_t
    {
    public:
        [[nodiscard]] movement_result_t update(world_t& world,
                                               movement_body_t& body,
                                               const movement_step_t& step) const override;
        [[nodiscard]] movement_result_t move(world_t& world,
                                             movement_body_t& body,
                                             chlm::float2 requested_delta) const override;
    };
} // namespace carrot::world
