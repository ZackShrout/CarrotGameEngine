//
// Created by zshro on 1/25/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Win32Window.h"

#include "Input/PlatformKeyMapping.h"

#include <wchar.h>

namespace carrot::core::platform {
    namespace {
        // Unique class name — use your project name or GUID in real code
        constexpr wchar_t WINDOW_CLASS_NAME[] = L"CarrotEngineWin32WindowClass";

        bool register_window_class(HINSTANCE hinst)
        {
            WNDCLASSEXW wc{ };
            wc.cbSize = sizeof(wc);
            wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = win32_window_t::WndProc;
            wc.cbClsExtra = 0;
            wc.cbWndExtra = 0;
            wc.hInstance = hinst;
            wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            wc.lpszMenuName = nullptr;
            wc.lpszClassName = WINDOW_CLASS_NAME;
            wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

            return RegisterClassExW(&wc) != 0;
        }
    } // anonymous namespace

    win32_window_t::win32_window_t(uint32_t width, uint32_t height, std::string_view title) noexcept : _width{ width },
        _height{ height }
    {
        LOG_CORE_INFO("Creating Win32 window: {}x{} \"{}\"", width, height, title);

        _title = std::wstring(title.begin(), title.end()); // safer

        _hinstance = GetModuleHandleW(nullptr);
        if (!_hinstance)
        {
            LOG_CORE_FATAL("GetModuleHandleW failed: {}", GetLastError());
            _should_close = true;
            return;
        }

        if (!register_window_class(_hinstance))
        {
            DWORD err = GetLastError();
            LOG_CORE_FATAL("RegisterClassExW failed with error {}", err);
            _should_close = true;
            return;
        }
        else
        {
            LOG_CORE_INFO("Window class registered successfully");
        }

        // Force class to exist check
        if (!GetClassInfoExW(_hinstance, WINDOW_CLASS_NAME, nullptr))
        {
            DWORD err = GetLastError(); // usually 1410 if exists, but 0 if not found? Wait no
            LOG_CORE_FATAL("GetClassInfoExW failed - class probably not registered: {}", err);
        }

        DWORD style = WS_OVERLAPPEDWINDOW;
        DWORD exstyle = WS_EX_APPWINDOW; // add this – helps visibility

        RECT rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        if (!AdjustWindowRectEx(&rect, style, FALSE, exstyle))
        {
            LOG_CORE_FATAL("AdjustWindowRectEx failed: {}", GetLastError());
        }

        LOG_CORE_INFO("Adjusted rect: left={}, top={}, right={}, bottom={}",
                      rect.left, rect.top, rect.right, rect.bottom);

        _hwnd = CreateWindowExW(
            exstyle,
            WINDOW_CLASS_NAME,
            _title.c_str(),
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr, nullptr,
            _hinstance,
            this
        );

        DWORD create_err = GetLastError(); // capture IMMEDIATELY
        if (!_hwnd)
        {
            LOG_CORE_FATAL("CreateWindowExW returned NULL - error code: {}", create_err);
            _should_close = true;
            return;
        }

        LOG_CORE_INFO("Window created! HWND = {:p}", (void*)_hwnd);

        ShowWindow(_hwnd, SW_SHOWDEFAULT);
        UpdateWindow(_hwnd);

        LOG_CORE_INFO("Win32 window created successfully (HWND = {:p})", (void*)_hwnd);
    }

    win32_window_t::~win32_window_t() noexcept
    {
        if (_hwnd)
        {
            DestroyWindow(_hwnd);
            _hwnd = nullptr;
        }
    }

    void win32_window_t::poll_events() noexcept
    {
        MSG msg{ };
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void win32_window_t::set_should_close(bool should_close) noexcept
    {
        _should_close = should_close;
        if (should_close && _hwnd)
            PostQuitMessage(0);
    }

    native_window_handle_t win32_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ };
        handle.win32_t.hwnd = _hwnd;
        handle.win32_t.hinstance = _hinstance;

        return handle;
    }

    LRESULT win32_window_t::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
    {
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        auto* self = reinterpret_cast<win32_window_t *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
            return self->handle_message(msg, wParam, lParam);

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // PRIVATE
    LRESULT win32_window_t::handle_message(UINT msg, WPARAM wParam, LPARAM lParam) noexcept
    {
        switch (msg)
        {
            case WM_DESTROY:
                _should_close = true;
                PostQuitMessage(0);
                return 0;
            case WM_CLOSE:
                _should_close = true;
                return 0;
            case WM_SIZE:
            {
                _width = LOWORD(lParam);
                _height = HIWORD(lParam);
                // TODO: later broadcast resize event if you add one
                return 0;
            }
            // struct key_event_t
            // {
            //     input::key_code _key{ 0 };
            //     key_action      _action{ key_action::press };
            //     bool            _repeat{ false };
            //     uint8_t         _mods{ 0 };                      // bitfield: shift/ctrl/alt/super
            // };
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            {
                const uint32_t vk{ static_cast<uint32_t>(wParam) };
                input::key_code key_code{ input::to_carrot_key(vk) };

                bool is_left{ false };
                bool is_right{ false };

                switch (vk)
                {
                    case VK_SHIFT:
                    {
                        const UINT scancode{ static_cast<UINT>(lParam >> 16 & 0xFF) }; // bits 16–23 = scan code
                        const UINT mapped_vk{ MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX) };

                        if (mapped_vk == VK_LSHIFT)
                        {
                            key_code = input::to_carrot_key(VK_LSHIFT);
                            is_left = true;
                        }
                        else if (mapped_vk == VK_RSHIFT)
                        {
                            key_code = input::to_carrot_key(VK_RSHIFT);
                            is_right = true;
                        }
                        break;
                    }
                    case VK_CONTROL:
                    {
                        const bool extended{ (lParam & 0x01000000) != 0 }; // bit 24
                        key_code = extended
                                       ? input::to_carrot_key(VK_RCONTROL)
                                       : input::to_carrot_key(VK_LCONTROL);
                        is_left = !extended;
                        is_right = extended;
                        break;
                    }
                    case VK_MENU:
                    {
                        const bool extended{ (lParam & 0x01000000) != 0 };
                        key_code = extended
                                       ? input::to_carrot_key(VK_RMENU)
                                       : input::to_carrot_key(VK_LMENU);
                        is_left = !extended;
                        is_right = extended;
                        break;
                    }
                    default:
                        break;
                }

                events::key_event_t e{ };
                e._key = key_code;
                e._action = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN
                                ? events::key_action::press
                                : events::key_action::release;
                e._repeat = ((lParam & 0x40000000) != 0); // bit 30 = previous state
                _on_key.broadcast(e);
                break;
            }
            // case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            // case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            // case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            // {
            //     events::mouse_button_event_t e{};
            //     e.button   = (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) ? 0 :
            //                  (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) ? 1 : 2;
            //     e.pressed  = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
            //                   msg == WM_MBUTTONDOWN);
            //     e.x        = LOWORD(lParam);
            //     e.y        = HIWORD(lParam);
            //     _on_mouse_button.broadcast(e);
            //     break;
            // }
            // case WM_MOUSEMOVE:
            // {
            //     events::mouse_moved_event_t e{};
            //     e.x = LOWORD(lParam);
            //     e.y = HIWORD(lParam);
            //     _on_mouse_moved.broadcast(e);
            //     break;
            // }
            //
            // case WM_MOUSEWHEEL:
            // {
            //     events::mouse_scrolled_event_t e{};
            //     e.delta = GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
            //     _on_mouse_scrolled.broadcast(e);
            //     break;
            // }
            default:
                break;
        }
        return DefWindowProcW(_hwnd, msg, wParam, lParam);
    }
} // namespace carrot::core::platform
