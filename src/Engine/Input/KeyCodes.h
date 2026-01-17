//
// Created by zshrout on 1/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::input {
    enum class key_code : uint16_t
    {
        unknown = 0,

        // Letters (A-Z) - uppercase semantic, but physical position
        a = 4, b = 5, c = 6, d = 7, e = 8, f = 9,
        g = 10, h = 11, i = 12, j = 13, k = 14, l = 15,
        m = 16, n = 17, o = 18, p = 19, q = 20, r = 21,
        s = 22, t = 23, u = 24, v = 25, w = 26, x = 27,
        y = 28, z = 29,

        // Numbers (top row)
        digit0 = 30, digit1 = 31, digit2 = 32, digit3 = 33, digit4 = 34,
        digit5 = 35, digit6 = 36, digit7 = 37, digit8 = 38, digit9 = 39,

        // Function keys
        f1 = 58, f2 = 59, f3 = 60, f4 = 61, f5 = 62,
        f6 = 63, f7 = 64, f8 = 65, f9 = 66, f10 = 67,
        f11 = 68, f12 = 69,

        // Special / navigation
        escape = 41,
        enter = 40,
        tab = 43,
        backspace = 42,
        space = 44,
        del = 76,
        insert = 73,

        // Arrows
        left = 80,
        right = 79,
        up = 82,
        down = 81,

        // Modifiers (used as bits in KeyEvent too, but here as keys)
        left_shift = 225,
        right_shift = 229,
        left_control = 224,
        right_control = 228,
        left_alt = 226,
        right_alt = 230,
        left_super = 227, // Win / Cmd
        right_super = 231,

        // More can be added: PageUp/Down, Home/End, CapsLock, etc.
        // Keep gaps for future expansion
        max_key_code = 512 // arbitrary upper bound
    };

    // Mouse buttons (simple enum)
    enum class mouse_button : uint8_t
    {
        unknown = 0,
        left = 1,
        right = 2,
        middle = 3,
        button4 = 4, // side/back
        button5 = 5, // side/forward
        // ... more if needed
    };
} // ns carrot::input
