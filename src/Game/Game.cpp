//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Game.h"

#include "Window/Window.h"

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

    void sandbox_t::on_key(const carrot::events::key_event_t& e)
    {
        ce_application_t::on_key(e);

        if (e._action == carrot::events::key_action::press)
            LOG_CORE_INFO("Key pressed: {}", static_cast<uint32_t>(e._key));
        else if (e._action == carrot::events::key_action::release)
            LOG_CORE_INFO("Key released: {}", static_cast<uint32_t>(e._key));

        if (e._action == carrot::events::key_action::press && e._key == carrot::input::key_code::escape)
            quit_application();
    }
} // namespace sandbox
