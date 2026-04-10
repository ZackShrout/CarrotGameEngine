//
// Created by zshrout on 1/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

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

            case core::platform::platform_type::x11:
            {
                switch (platform_code)
                {
                    case 0x0041:
                    case 0x0061: return key_code::a;           // XK_A / XK_a
                    case 0x0042:
                    case 0x0062: return key_code::b;           // XK_B / XK_b
                    case 0x0043:
                    case 0x0063: return key_code::c;           // XK_C / XK_c
                    case 0x0044:
                    case 0x0064: return key_code::d;           // XK_D / XK_d
                    case 0x0045:
                    case 0x0065: return key_code::e;           // XK_E / XK_e
                    case 0x0046:
                    case 0x0066: return key_code::f;           // XK_F / XK_f
                    case 0x0047:
                    case 0x0067: return key_code::g;           // XK_G / XK_g
                    case 0x0048:
                    case 0x0068: return key_code::h;           // XK_H / XK_h
                    case 0x0049:
                    case 0x0069: return key_code::i;           // XK_I / XK_i
                    case 0x004A:
                    case 0x006A: return key_code::j;           // XK_J / XK_j
                    case 0x004B:
                    case 0x006B: return key_code::k;           // XK_K / XK_k
                    case 0x004C:
                    case 0x006C: return key_code::l;           // XK_L / XK_l
                    case 0x004D:
                    case 0x006D: return key_code::m;           // XK_M / XK_m
                    case 0x004E:
                    case 0x006E: return key_code::n;           // XK_N / XK_n
                    case 0x004F:
                    case 0x006F: return key_code::o;           // XK_O / XK_o
                    case 0x0050:
                    case 0x0070: return key_code::p;           // XK_P / XK_p
                    case 0x0051:
                    case 0x0071: return key_code::q;           // XK_Q / XK_q
                    case 0x0052:
                    case 0x0072: return key_code::r;           // XK_R / XK_r
                    case 0x0053:
                    case 0x0073: return key_code::s;           // XK_S / XK_s
                    case 0x0054:
                    case 0x0074: return key_code::t;           // XK_T / XK_t
                    case 0x0055:
                    case 0x0075: return key_code::u;           // XK_U / XK_u
                    case 0x0056:
                    case 0x0076: return key_code::v;           // XK_V / XK_v
                    case 0x0057:
                    case 0x0077: return key_code::w;           // XK_W / XK_w
                    case 0x0058:
                    case 0x0078: return key_code::x;           // XK_X / XK_x
                    case 0x0059:
                    case 0x0079: return key_code::y;           // XK_Y / XK_y
                    case 0x005A:
                    case 0x007A: return key_code::z;           // XK_Z / XK_z

                    case 0x0030: return key_code::digit0;      // XK_0
                    case 0x0031: return key_code::digit1;      // XK_1
                    case 0x0032: return key_code::digit2;      // XK_2
                    case 0x0033: return key_code::digit3;      // XK_3
                    case 0x0034: return key_code::digit4;      // XK_4
                    case 0x0035: return key_code::digit5;      // XK_5
                    case 0x0036: return key_code::digit6;      // XK_6
                    case 0x0037: return key_code::digit7;      // XK_7
                    case 0x0038: return key_code::digit8;      // XK_8
                    case 0x0039: return key_code::digit9;      // XK_9

                    case 0xFFBE: return key_code::f1;          // XK_F1
                    case 0xFFBF: return key_code::f2;          // XK_F2
                    case 0xFFC0: return key_code::f3;          // XK_F3
                    case 0xFFC1: return key_code::f4;          // XK_F4
                    case 0xFFC2: return key_code::f5;          // XK_F5
                    case 0xFFC3: return key_code::f6;          // XK_F6
                    case 0xFFC4: return key_code::f7;          // XK_F7
                    case 0xFFC5: return key_code::f8;          // XK_F8
                    case 0xFFC6: return key_code::f9;          // XK_F9
                    case 0xFFC7: return key_code::f10;         // XK_F10
                    case 0xFFC8: return key_code::f11;         // XK_F11
                    case 0xFFC9: return key_code::f12;         // XK_F12

                    case 0xFF1B: return key_code::escape;      // XK_Escape
                    case 0xFF0D:
                    case 0xFF8D: return key_code::enter;       // XK_Return / XK_KP_Enter
                    case 0xFF09: return key_code::tab;         // XK_Tab
                    case 0xFF08: return key_code::backspace;   // XK_BackSpace
                    case 0x0020: return key_code::space;       // XK_space
                    case 0xFFFF: return key_code::del;         // XK_Delete
                    case 0xFF63: return key_code::insert;      // XK_Insert

                    case 0xFF51: return key_code::left;        // XK_Left
                    case 0xFF53: return key_code::right;       // XK_Right
                    case 0xFF52: return key_code::up;          // XK_Up
                    case 0xFF54: return key_code::down;        // XK_Down

                    case 0xFFE1: return key_code::left_shift;  // XK_Shift_L
                    case 0xFFE2: return key_code::right_shift; // XK_Shift_R
                    case 0xFFE3: return key_code::left_control;// XK_Control_L
                    case 0xFFE4: return key_code::right_control;// XK_Control_R
                    case 0xFFE9:
                    case 0xFFE7: return key_code::left_alt;    // XK_Alt_L / XK_Meta_L
                    case 0xFFEA:
                    case 0xFFE8: return key_code::right_alt;   // XK_Alt_R / XK_Meta_R
                    case 0xFFEB: return key_code::left_super;  // XK_Super_L
                    case 0xFFEC: return key_code::right_super; // XK_Super_R

                    default: return key_code::unknown;
                }
            }

            case core::platform::platform_type::win32:
            {
                switch (platform_code)
                {
                    case 0x41: return key_code::a;              // VK_A
                    case 0x42: return key_code::b;              // VK_B
                    case 0x43: return key_code::c;              // VK_C
                    case 0x44: return key_code::d;              // VK_D
                    case 0x45: return key_code::e;              // VK_E
                    case 0x46: return key_code::f;              // VK_F
                    case 0x47: return key_code::g;              // VK_G
                    case 0x48: return key_code::h;              // VK_H
                    case 0x49: return key_code::i;              // VK_I
                    case 0x4A: return key_code::j;              // VK_J
                    case 0x4B: return key_code::k;              // VK_K
                    case 0x4C: return key_code::l;              // VK_L
                    case 0x4D: return key_code::m;              // VK_M
                    case 0x4E: return key_code::n;              // VK_N
                    case 0x4F: return key_code::o;              // VK_O
                    case 0x50: return key_code::p;              // VK_P
                    case 0x51: return key_code::q;              // VK_Q
                    case 0x52: return key_code::r;              // VK_R
                    case 0x53: return key_code::s;              // VK_S
                    case 0x54: return key_code::t;              // VK_T
                    case 0x55: return key_code::u;              // VK_U
                    case 0x56: return key_code::v;              // VK_V
                    case 0x57: return key_code::w;              // VK_W
                    case 0x58: return key_code::x;              // VK_X
                    case 0x59: return key_code::y;              // VK_Y
                    case 0x5A: return key_code::z;              // VK_Z

                    case 0x30: return key_code::digit0;         // VK_0
                    case 0x31: return key_code::digit1;         // VK_1
                    case 0x32: return key_code::digit2;         // VK_2
                    case 0x33: return key_code::digit3;         // VK_3
                    case 0x34: return key_code::digit4;         // VK_4
                    case 0x35: return key_code::digit5;         // VK_5
                    case 0x36: return key_code::digit6;         // VK_6
                    case 0x37: return key_code::digit7;         // VK_7
                    case 0x38: return key_code::digit8;         // VK_8
                    case 0x39: return key_code::digit9;         // VK_9

                    case 0x70: return key_code::f1;             // VK_F1
                    case 0x71: return key_code::f2;             // VK_F2
                    case 0x72: return key_code::f3;             // VK_F3
                    case 0x73: return key_code::f4;             // VK_F4
                    case 0x74: return key_code::f5;             // VK_F5
                    case 0x75: return key_code::f6;             // VK_F6
                    case 0x76: return key_code::f7;             // VK_F7
                    case 0x77: return key_code::f8;             // VK_F8
                    case 0x78: return key_code::f9;             // VK_F9
                    case 0x79: return key_code::f10;            // VK_F10
                    case 0x7A: return key_code::f11;            // VK_F11
                    case 0x7B: return key_code::f12;            // VK_F12

                    case 0x1B: return key_code::escape;         // VK_ESCAPE
                    case 0x0D: return key_code::enter;          // VK_RETURN
                    case 0x09: return key_code::tab;            // VK_TAB
                    case 0x08: return key_code::backspace;      // VK_BACK
                    case 0x20: return key_code::space;          // VK_SPACE
                    case 0x2E: return key_code::del;            // VK_DELETE
                    case 0x2D: return key_code::insert;         // VK_INSERT

                    case 0x25: return key_code::left;           // VK_LEFT
                    case 0x27: return key_code::right;          // VK_RIGHT
                    case 0x26: return key_code::up;             // VK_UP
                    case 0x28: return key_code::down;           // VK_DOWN

                    case 0xA0: return key_code::left_shift;     // VK_LSHIFT
                    case 0xA1: return key_code::right_shift;    // VK_RSHIFT
                    case 0xA2: return key_code::left_control;   // VK_LCONTROL
                    case 0xA3: return key_code::right_control;  // VK_RCONTROL
                    case 0xA4: return key_code::left_alt;       // VK_LMENU
                    case 0xA5: return key_code::right_alt;      // VK_RMENU
                    case 0x5B: return key_code::left_super;     // VK_LWIN
                    case 0x5C: return key_code::right_super;    // VK_RWIN

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
                if (platform_button == 0x113) return mouse_button::button4;
                if (platform_button == 0x114) return mouse_button::button5;
                break;
            case core::platform::platform_type::x11:
                if (platform_button == 0x01) return mouse_button::left;
                if (platform_button == 0x02) return mouse_button::middle;
                if (platform_button == 0x03) return mouse_button::right;
                if (platform_button == 0x08) return mouse_button::button4;
                if (platform_button == 0x09) return mouse_button::button5;
                break;
            case core::platform::platform_type::win32:
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
