//
// Created by zshrout on 1/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Input/KeyCodes.h"

#include <chlm/CarrotHLM.h>
#include <cstdint>

namespace carrot::events {
    // ───────────────────────────────────────────────
    // Window / Application level
    // ───────────────────────────────────────────────

    struct window_resized_t
    {
        uint32_t _width{ 0 };
        uint32_t _height{ 0 };
    };

    struct window_closed_t {};

    struct window_focused_t
    {
        bool _focused{ true };
    };

    // ───────────────────────────────────────────────
    // Input – keyboard
    // ───────────────────────────────────────────────

    enum class key_action : uint8_t
    {
        press = 0,
        release = 1,
        repeat = 2
    };

    struct key_event_t
    {
        input::key_code _key{ 0 };
        key_action      _action{ key_action::press };
        bool            _repeat{ false };
        uint8_t         _mods{ 0 };                      // bitfield: shift/ctrl/alt/super
    };

    // ───────────────────────────────────────────────
    // Input – mouse
    // ───────────────────────────────────────────────

    enum class mouse_button : uint8_t
    {
        left = 0,
        right = 1,
        middle = 2,
    };

    struct mouse_button_event_t
    {
        input::mouse_button _button{ input::mouse_button::left };
        key_action          _action{ key_action::press };
        chlm::float2        _pos{ 0.f, 0.f };                // absolute window coordinates
    };

    struct mouse_moved_event_t
    {
        chlm::float2 _pos{ 0.f, 0.f };       // current absolute position
        chlm::float2 _delta{ 0.f, 0.f };     // movement since last event
    };

    struct mouse_scrolled_event_t
    {
        chlm::float2 _delta{ 0.f, 0.f };     // usually (0.f, scroll_y)
    };
} // ns
