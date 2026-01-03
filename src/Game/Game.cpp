//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#include "Game.h"

namespace sandbox {
    void sandbox_t::on_tick(const float delta_time)
    {
        // Just some silly stuff to show ourselves that the on_tick function is hooked up from within the engine
        static float seconds_counter{ 0.0f };
        static int seconds{ 0 };
        seconds_counter += delta_time;

        if (seconds_counter >= 1.0f)
        {
            seconds++;
            seconds_counter = 0.0f;
            LOG_CORE_INFO("Seconds: {}", seconds);
        }
    }
} // namespace sandbox
