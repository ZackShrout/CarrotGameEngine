//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

#include "WorldInteractionHelpers.h"

namespace sandbox {
    class sandbox_interaction_controller_t final : public carrot::world::interaction_controller_t
    {
    public:
        [[nodiscard]] std::optional<scene_transition_request_t> consume_pending_transition() noexcept;

    protected:
        void on_interact(carrot::core::game_context_t& game,
                         const carrot::world::world_object_t& object) override;

    private:
        std::optional<scene_transition_request_t> _pending_transition;
    };
} // namespace sandbox
