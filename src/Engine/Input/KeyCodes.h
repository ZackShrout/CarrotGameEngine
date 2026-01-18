//
// Created by zshrout on 1/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>

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

    enum class modifier : uint8_t
    {
        none = 0,
        shift = 1 << 0,
        control = 1 << 1,
        alt = 1 << 2,
        super = 1 << 3,
    };

    constexpr bool has_modifier(const uint8_t mods, modifier m) noexcept
    {
        return (mods & static_cast<uint8_t>(m)) != 0;
    }

    constexpr const char* key_code_to_string(const key_code key)
    {
        switch (key)
        {
            case key_code::a: return "A";
            case key_code::b: return "B";
            case key_code::c: return "C";
            case key_code::d: return "D";
            case key_code::e: return "E";
            case key_code::f: return "F";
            case key_code::g: return "G";
            case key_code::h: return "H";
            case key_code::i: return "I";
            case key_code::j: return "J";
            case key_code::k: return "K";
            case key_code::l: return "L";
            case key_code::m: return "M";
            case key_code::n: return "N";
            case key_code::o: return "O";
            case key_code::p: return "P";
            case key_code::q: return "Q";
            case key_code::r: return "R";
            case key_code::s: return "S";
            case key_code::t: return "T";
            case key_code::u: return "U";
            case key_code::v: return "V";
            case key_code::w: return "W";
            case key_code::x: return "X";
            case key_code::y: return "Y";
            case key_code::z: return "Z";

            case key_code::digit0: return "0";
            case key_code::digit1: return "1";
            case key_code::digit2: return "2";
            case key_code::digit3: return "3";
            case key_code::digit4: return "4";
            case key_code::digit5: return "5";
            case key_code::digit6: return "6";
            case key_code::digit7: return "7";
            case key_code::digit8: return "8";
            case key_code::digit9: return "9";

            case key_code::f1: return "F1";
            case key_code::f2: return "F2";
            case key_code::f3: return "F3";
            case key_code::f4: return "F4";
            case key_code::f5: return "F5";
            case key_code::f6: return "F6";
            case key_code::f7: return "F7";
            case key_code::f8: return "F8";
            case key_code::f9: return "F9";
            case key_code::f10: return "F10";
            case key_code::f11: return "F11";
            case key_code::f12: return "F12";

            case key_code::escape: return "Escape";
            case key_code::enter: return "Enter";
            case key_code::tab: return "Tab";
            case key_code::backspace: return "Backspace";
            case key_code::space: return "Space";
            case key_code::del: return "Delete";
            case key_code::insert: return "Insert";

            case key_code::left: return "Left Arrow";
            case key_code::right: return "Right Arrow";
            case key_code::up: return "Up Arrow";
            case key_code::down: return "Down Arrow";

            case key_code::left_shift: return "Left Shift";
            case key_code::right_shift: return "Right Shift";
            case key_code::left_control: return "Left Control";
            case key_code::right_control: return "Right Control";
            case key_code::left_alt: return "Left Alt";
            case key_code::right_alt: return "Right Alt";
            case key_code::left_super: return "Left Super";
            case key_code::right_super: return "Right Super";

            default: return "Unknown";
        }
    }

    constexpr const char* mouse_button_to_string(const mouse_button button)
    {
        switch (button)
        {
            case mouse_button::left: return "Left";
            case mouse_button::right: return "Right";
            case mouse_button::middle: return "Middle";
            case mouse_button::button4: return "Button4";
            case mouse_button::button5: return "Button5";
            default: return "Unknown";
        }
    }

    constexpr std::string modifiers_to_string(const uint8_t mods)
    {
        std::string s;

        if (has_modifier(mods, modifier::shift)) s += "Shift ";
        if (has_modifier(mods, modifier::control)) s += "Ctrl ";
        if (has_modifier(mods, modifier::alt)) s += "Alt ";
        if (has_modifier(mods, modifier::super)) s += "Super ";
        if (s.empty()) return "None";

        s.pop_back();
        return s;
    }
} // ns carrot::input
