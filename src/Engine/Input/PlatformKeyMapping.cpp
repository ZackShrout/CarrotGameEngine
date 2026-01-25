//
// Created by zshrout on 1/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "PlatformKeyMapping.h"

#include "Core/Platform/Platform.h"

namespace carrot::input {
    [[nodiscard]] key_code to_carrot_key(const uint32_t platform_code) noexcept
    {
        switch (core::platform::current_platform())
        {
            case core::platform::platform_type::wayland:
            {
                switch (platform_code)
                {
                    case 0x1E: return key_code::a;              // KEY_A
                    case 0x30: return key_code::b;              // KEY_B
                    case 0x2E: return key_code::c;              // KEY_C
                    case 0x20: return key_code::d;              // KEY_D
                    case 0x12: return key_code::e;              // KEY_E
                    case 0x21: return key_code::f;              // KEY_F
                    case 0x22: return key_code::g;              // KEY_G
                    case 0x23: return key_code::h;              // KEY_H
                    case 0x17: return key_code::i;              // KEY_I
                    case 0x24: return key_code::j;              // KEY_J
                    case 0x25: return key_code::k;              // KEY_K
                    case 0x26: return key_code::l;              // KEY_L
                    case 0x32: return key_code::m;              // KEY_M
                    case 0x31: return key_code::n;              // KEY_N
                    case 0x18: return key_code::o;              // KEY_O
                    case 0x19: return key_code::p;              // KEY_P
                    case 0x10: return key_code::q;              // KEY_Q
                    case 0x13: return key_code::r;              // KEY_R
                    case 0x1F: return key_code::s;              // KEY_S
                    case 0x14: return key_code::t;              // KEY_T
                    case 0x16: return key_code::u;              // KEY_U
                    case 0x2F: return key_code::v;              // KEY_V
                    case 0x11: return key_code::w;              // KEY_W
                    case 0x2D: return key_code::x;              // KEY_X
                    case 0x15: return key_code::y;              // KEY_Y
                    case 0x2C: return key_code::z;              // KEY_Z

                    case 0x0B: return key_code::digit0;         // KEY_0
                    case 0x02: return key_code::digit1;         // KEY_1
                    case 0x03: return key_code::digit2;         // KEY_2
                    case 0x04: return key_code::digit3;         // KEY_3
                    case 0x05: return key_code::digit4;         // KEY_4
                    case 0x06: return key_code::digit5;         // KEY_5
                    case 0x07: return key_code::digit6;         // KEY_6
                    case 0x08: return key_code::digit7;         // KEY_7
                    case 0x09: return key_code::digit8;         // KEY_8
                    case 0x0A: return key_code::digit9;         // KEY_9

                    case 0x3B: return key_code::f1;             // KEY_F1
                    case 0x3C: return key_code::f2;             // KEY_F2
                    case 0x3D: return key_code::f3;             // KEY_F3
                    case 0x3E: return key_code::f4;             // KEY_F4
                    case 0x3F: return key_code::f5;             // KEY_F5
                    case 0x40: return key_code::f6;             // KEY_F6
                    case 0x41: return key_code::f7;             // KEY_F7
                    case 0x42: return key_code::f8;             // KEY_F8
                    case 0x43: return key_code::f9;             // KEY_F9
                    case 0x44: return key_code::f10;            // KEY_F10
                    case 0x57: return key_code::f11;            // KEY_F11
                    case 0x58: return key_code::f12;            // KEY_F12

                    case 0x01: return key_code::escape;         // KEY_ESC
                    case 0x1C: return key_code::enter;          // KEY_ENTER
                    case 0x0F: return key_code::tab;            // KEY_TAB
                    case 0x0E: return key_code::backspace;      // KEY_BACKSPACE
                    case 0x39: return key_code::space;          // KEY_SPACE
                    case 0x6F: return key_code::del;            // KEY_DELETE
                    case 0x6E: return key_code::insert;         // KEY_INSERT

                    case 0x69: return key_code::left;           // KEY_LEFT
                    case 0x6A: return key_code::right;          // KEY_RIGHT
                    case 0x67: return key_code::up;             // KEY_UP
                    case 0x6C: return key_code::down;           // KEY_DOWN

                    case 0x2A: return key_code::left_shift;     // KEY_LEFTSHIFT
                    case 0x36: return key_code::right_shift;    // KEY_RIGHTSHIFT
                    case 0x1D: return key_code::left_control;   // KEY_LEFTCTRL
                    case 0x61: return key_code::right_control;  // KEY_RIGHTCTRL
                    case 0x38: return key_code::left_alt;       // KEY_LEFTALT
                    case 0x64: return key_code::right_alt;      // KEY_RIGHTALT
                    case 0x7D: return key_code::left_super;     // KEY_LEFTMETA
                    case 0x7E: return key_code::right_super;    // KEY_RIGHTMETA

                    default: return key_code::unknown;
                }
            }

            case core::platform::platform_type::win32:
            {
                switch (platform_code)
                {
                    case 0x41: return key_code::a; // VK_A
                    case 0x42: return key_code::b; // VK_B
                    // VK_A = 0x41, VK_B = 0x42, etc. for letters
                    case 0x30: return key_code::digit0; // VK_0
                    case 0x31: return key_code::digit1; // VK_1

                    case 0x1B: return key_code::escape; // VK_ESCAPE
                    case 0x0D: return key_code::enter; // VK_RETURN

                    case 0x25: return key_code::left; // VK_LEFT
                    case 0x27: return key_code::right; // VK_RIGHT
                    case 0x26: return key_code::up; // VK_UP
                    case 0x28: return key_code::down; // VK_DOWN

                    case 0xA0: return key_code::left_shift; // VK_LSHIFT
                    case 0xA1: return key_code::right_shift; // VK_RSHIFT
                    case 0xA2: return key_code::left_control; // VK_LCONTROL
                    case 0xA3: return key_code::right_control; // VK_RCONTROL
                    case 0xA4: return key_code::left_alt; // VK_LMENU
                    case 0xA5: return key_code::right_alt; // VK_RMENU
                    case 0x5B: return key_code::left_super; // VK_LWIN
                    case 0x5C: return key_code::right_super; // VK_RWIN

                    default: return key_code::unknown;
                }
            }

            case core::platform::platform_type::cocoa:
            {
                switch (platform_code)
                {
                    case 0x00: return key_code::a;              // kVK_ANSI_A
                    case 0x0B: return key_code::b;              // kVK_ANSI_B
                    case 0x08: return key_code::c;              // kVK_ANSI_C
                    case 0x02: return key_code::d;              // kVK_ANSI_D
                    case 0x0E: return key_code::e;              // kVK_ANSI_E
                    case 0x03: return key_code::f;              // kVK_ANSI_F
                    case 0x05: return key_code::g;              // kVK_ANSI_G
                    case 0x04: return key_code::h;              // kVK_ANSI_H
                    case 0x22: return key_code::i;              // kVK_ANSI_I
                    case 0x26: return key_code::j;              // kVK_ANSI_J
                    case 0x28: return key_code::k;              // kVK_ANSI_K
                    case 0x25: return key_code::l;              // kVK_ANSI_L
                    case 0x2E: return key_code::m;              // kVK_ANSI_M
                    case 0x2D: return key_code::n;              // kVK_ANSI_N
                    case 0x1F: return key_code::o;              // kVK_ANSI_O
                    case 0x23: return key_code::p;              // kVK_ANSI_P
                    case 0x0C: return key_code::q;              // kVK_ANSI_Q
                    case 0x0F: return key_code::r;              // kVK_ANSI_R
                    case 0x01: return key_code::s;              // kVK_ANSI_S
                    case 0x11: return key_code::t;              // kVK_ANSI_T
                    case 0x20: return key_code::u;              // kVK_ANSI_U
                    case 0x09: return key_code::v;              // kVK_ANSI_V
                    case 0x0D: return key_code::w;              // kVK_ANSI_W
                    case 0x07: return key_code::x;              // kVK_ANSI_X
                    case 0x10: return key_code::y;              // kVK_ANSI_Y
                    case 0x06: return key_code::z;              // kVK_ANSI_Z

                    case 0x1D: return key_code::digit0;         // kVK_ANSI_0
                    case 0x12: return key_code::digit1;         // kVK_ANSI_1
                    case 0x13: return key_code::digit2;         // kVK_ANSI_2
                    case 0x14: return key_code::digit3;         // kVK_ANSI_3
                    case 0x15: return key_code::digit4;         // kVK_ANSI_4
                    case 0x17: return key_code::digit5;         // kVK_ANSI_5
                    case 0x16: return key_code::digit6;         // kVK_ANSI_6
                    case 0x1A: return key_code::digit7;         // kVK_ANSI_7
                    case 0x1C: return key_code::digit8;         // kVK_ANSI_8
                    case 0x19: return key_code::digit9;         // kVK_ANSI_9

                    case 0x7A: return key_code::f1;             // kVK_F1
                    case 0x78: return key_code::f2;             // kVK_F2
                    case 0x63: return key_code::f3;             // kVK_F3
                    case 0x76: return key_code::f4;             // kVK_F4
                    case 0x60: return key_code::f5;             // kVK_F5
                    case 0x61: return key_code::f6;             // kVK_F6
                    case 0x62: return key_code::f7;             // kVK_F7
                    case 0x64: return key_code::f8;             // kVK_F8
                    case 0x65: return key_code::f9;             // kVK_F9
                    case 0x6D: return key_code::f10;            // kVK_F10
                    case 0x67: return key_code::f11;            // kVK_F11
                    case 0x6F: return key_code::f12;            // kVK_F12

                    case 0x35: return key_code::escape;         // kVK_Escape
                    case 0x24: return key_code::enter;          // kVK_Return
                    case 0x30: return key_code::tab;            // kVK_Tab
                    case 0x33: return key_code::backspace;      // kVK_Delete
                    case 0x31: return key_code::space;          // kVK_Space
                    case 0x75: return key_code::del;            // kVK_ForwardDelete
                    case 0x72: return key_code::insert;         // kVK_Help

                    case 0x7B: return key_code::left;           // kVK_LeftArrow
                    case 0x7C: return key_code::right;          // kVK_RightArrow
                    case 0x7E: return key_code::up;             // kVK_UpArrow
                    case 0x7D: return key_code::down;           // kVK_DownArrow

                    case 0x38: return key_code::left_shift;     // kVK_Shift
                    case 0x3C: return key_code::right_shift;    // kVK_RightShift
                    case 0x3B: return key_code::left_control;   // kVK_Control
                    case 0x3E: return key_code::right_control;  // kVK_RightControl
                    case 0x3A: return key_code::left_alt;       // kVK_Option
                    case 0x3D: return key_code::right_alt;      // kVK_RightOption
                    case 0x37: return key_code::left_super;     // kVK_Command
                    case 0x36: return key_code::right_super;    // kVK_RightCommand

                    default: return key_code::unknown;
                }
            }

            default:
                return key_code::unknown;
        }
    }

    mouse_button to_carrot_mouse_button(const uint32_t platform_button) noexcept
    {
        switch (core::platform::current_platform())
        {
            case core::platform::platform_type::wayland:
                if (platform_button == 0x110) return mouse_button::left; // BTN_LEFT
                if (platform_button == 0x111) return mouse_button::right; // BTN_RIGHT
                if (platform_button == 0x112) return mouse_button::middle; // BTN_MIDDLE
                break;
            case core::platform::platform_type::win32:
                if (platform_button == 0x00) return mouse_button::left;
                if (platform_button == 0x01) return mouse_button::right;
                if (platform_button == 0x02) return mouse_button::middle;
                break;
            case core::platform::platform_type::cocoa:
                if (platform_button == 0x00) return mouse_button::left;
                if (platform_button == 0x01) return mouse_button::right;
                if (platform_button == 0x02) return mouse_button::middle;
                break;
            default:
                break;
        }
        return mouse_button::unknown;
    }
} // namespace carrot::input
