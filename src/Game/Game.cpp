//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Game.h"

#include "Audio/Audio.h"
#include "Window/Window.h"

namespace sandbox {
    namespace {
        carrot::audio::voice_handle_t handle;

        // std::string test_asset_name{ "music.victory" };
        std::string test_asset_name{ "music.jalen_theme" };
    }

    void sandbox_t::start()
    {
        handle = carrot::audio::play(test_asset_name);
    }

    void sandbox_t::on_tick(const float delta_time)
    {
        // Just some silly stuff to show ourselves that the on_tick function is hooked up from within the engine
        static float seconds_counter{ 0.0f };
        static int seconds{ 0 };
        static bool played{ false };
        seconds_counter += delta_time;

        if (seconds_counter >= 1.0f)
        {
            seconds++;
            seconds_counter = 0.0f;
            LOG_CORE_INFO("Seconds: {}", seconds);
        }

        if (seconds == 10)
            carrot::audio::stop(handle);

        if (seconds == 11 && !played)
        {
            handle = carrot::audio::play(test_asset_name);
            played = true;
        }

        if (seconds == 16)
            carrot::audio::pause(handle);

        if (seconds == 21)
            carrot::audio::resume(handle);
    }

    void sandbox_t::on_key(const carrot::events::key_event_t& e)
    {
        ce_application_t::on_key(e);

        if (e._action == carrot::events::key_action::press)
        {
            LOG_CORE_INFO("Key pressed: {} ({}) (mods: {})", carrot::input::key_code_to_string(e._key),
                          static_cast<uint32_t>(e._key), carrot::input::modifiers_to_string(e._mods));

            if (e._key == carrot::input::key_code::a)
            {
                if (carrot::input::has_modifier(e._mods, carrot::input::modifier::shift))
                    LOG_CORE_INFO("Shift+A -> Uppercase!");
                else
                    LOG_CORE_INFO("Just A");
            }
        }
        else if (e._action == carrot::events::key_action::repeat)
            LOG_CORE_INFO("Key held: {} ({})", carrot::input::key_code_to_string(e._key),
                      static_cast<uint32_t>(e._key));
        else if (e._action == carrot::events::key_action::release)
            LOG_CORE_INFO("Key released: {} ({})", carrot::input::key_code_to_string(e._key),
                      static_cast<uint32_t>(e._key));

        if (e._action == carrot::events::key_action::press && e._key == carrot::input::key_code::escape)
            quit_application();

        if (e._action == carrot::events::key_action::press && (
                (e._key == carrot::input::key_code::enter && carrot::input::has_modifier(
                     e._mods, carrot::input::modifier::alt)) || e._key == carrot::input::key_code::f11))
        {
            set_fullscreen(!is_fullscreen());
        }
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
