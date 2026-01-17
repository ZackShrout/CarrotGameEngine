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
            LOG_CORE_INFO("Key pressed: {} ({})", carrot::input::key_code_to_string(e._key),
                      static_cast<uint32_t>(e._key));
        else if (e._action == carrot::events::key_action::release)
            LOG_CORE_INFO("Key released: {} ({})", carrot::input::key_code_to_string(e._key),
                      static_cast<uint32_t>(e._key));

        if (e._action == carrot::events::key_action::press && e._key == carrot::input::key_code::escape)
            quit_application();
    }

    void sandbox_t::on_mouse_moved(const carrot::events::mouse_moved_event_t& e)
    {
        ce_application_t::on_mouse_moved(e);

        static int move_counter = 0;
        if (++move_counter % 5 == 0)
        {
            LOG_CORE_TRACE("Mouse: {:.0f}, {:.0f} (delta {:.1f}, {:.1f})", e._pos.x, e._pos.y, e._delta.x, e._delta.y);
        }
    }

    void sandbox_t::on_mouse_button(const carrot::events::mouse_button_event_t& e)
    {
        ce_application_t::on_mouse_button(e);

        if (e._action == carrot::events::key_action::press)
            LOG_CORE_INFO("Mouse Button {} ({}) pressed", carrot::input::mouse_button_to_string(e._button),
                      static_cast<uint32_t>(e._button));
        else if (e._action == carrot::events::key_action::release)
            LOG_CORE_INFO("Mouse Button {} ({}) released", carrot::input::mouse_button_to_string(e._button),
                      static_cast<uint32_t>(e._button));
    }

    void sandbox_t::on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e)
    {
        ce_application_t::on_mouse_scrolled(e);

        LOG_CORE_INFO("Mouse wheel scrolled: {} {}", static_cast<int32_t>(e._delta.x),
                      static_cast<int32_t>(e._delta.y));
    }
} // namespace sandbox
