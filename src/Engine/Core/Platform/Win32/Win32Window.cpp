//
// Created by zshro on 1/25/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Win32Window.h"

#include "Input/PlatformKeyMapping.h"

#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace carrot::core::platform {
    namespace {
        // Unique class name — could also use project name or GUID in real code
        constexpr wchar_t window_class_name[]{ L"CarrotEngineWin32WindowClass" };

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
            wc.lpszClassName = window_class_name;
            wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

            return RegisterClassExW(&wc) != 0;
        }

        bool is_system_dark_mode() noexcept
        {
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                              0,
                              KEY_READ,
                              &hKey) != ERROR_SUCCESS)
            {
                return false; // fallback to light if can't read
            }

            DWORD value{ 1 }; // default to light
            DWORD size{ sizeof(DWORD) };
            RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<BYTE *>(&value), &size);
            RegCloseKey(hKey);

            return value == 0; // 0 = dark
        }
    } // anonymous namespace

    win32_window_t::win32_window_t(const uint32_t width, const uint32_t height, std::string_view title) noexcept
    {
        _width = width;
        _height = height;
        _title = std::wstring(title.begin(), title.end());

        _hinstance = GetModuleHandleW(nullptr);
        if (!_hinstance)
        {
            LOG_CORE_FATAL("GetModuleHandleW failed: {}", GetLastError());
            _should_close = true;
            return;
        }

        if (!register_window_class(_hinstance))
        {
            DWORD err{ GetLastError() };
            LOG_CORE_FATAL("RegisterClassExW failed with error {}", err);
            _should_close = true;
            return;
        }

        constexpr DWORD style{ WS_OVERLAPPEDWINDOW };
        constexpr DWORD ex_style{ WS_EX_APPWINDOW };

        RECT rect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

        if (!AdjustWindowRectEx(&rect, style, FALSE, ex_style))
            LOG_CORE_ERROR("AdjustWindowRectEx failed: {}", GetLastError());

        _hwnd = CreateWindowExW(
            ex_style,
            window_class_name,
            _title.c_str(),
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr, nullptr,
            _hinstance,
            this
        );

        DWORD create_err{ GetLastError() };
        if (!_hwnd)
        {
            LOG_CORE_FATAL("CreateWindowExW returned NULL - error code: {}", create_err);
            _should_close = true;
            return;
        }

        const BOOL use_dark{ is_system_dark_mode() ? TRUE : FALSE };
        HRESULT hr{DwmSetWindowAttribute(_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &use_dark, sizeof(BOOL)) };

        if (!SUCCEEDED(hr))
        {
            LOG_CORE_WARN("DwmSetWindowAttribute failed (HRESULT = {:#x}) - title bar theme may not match system preference", hr);

            if (hr == E_NOTIMPL)
            {
                LOG_CORE_WARN("  → DWMWA_USE_IMMERSIVE_DARK_MODE not supported (pre-Win10 1809?)");
            }
            else if (hr == E_INVALIDARG)
            {
                LOG_CORE_ERROR("  → Invalid argument passed to DwmSetWindowAttribute");
            }
        }

        // Force DWM to redraw / refresh non-client area
        SetWindowPos(_hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        ShowWindow(_hwnd, SW_SHOWDEFAULT);
        UpdateWindow(_hwnd);
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

    void win32_window_t::set_should_close(const bool should_close) noexcept
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

    void win32_window_t::set_fullscreen(const bool fullscreen) noexcept
    {
        if (fullscreen == _is_fullscreen) return;
        if (!_hwnd) return;

        _is_fullscreen = fullscreen;

        if (fullscreen)
        {
            // Going → fullscreen

            // Remember current state so we can restore it perfectly
            GetWindowRect(_hwnd, &_prev_window_rect);
            _prev_style    = GetWindowLongPtrW(_hwnd, GWL_STYLE);
            _prev_ex_style = GetWindowLongPtrW(_hwnd, GWL_EXSTYLE);

            // Optional: was it maximized before we mess with it?
            WINDOWPLACEMENT wp{};
            wp.length = sizeof(wp);
            GetWindowPlacement(_hwnd, &wp);
            _was_maximized = (wp.showCmd == SW_SHOWMAXIMIZED);

            // Remove title bar, borders, etc.
            SetWindowLongPtrW(_hwnd, GWL_STYLE,
                _prev_style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU));

            // Usually keep WS_EX_APPWINDOW so it appears in taskbar/alt-tab
            // (remove WS_EX_TOPMOST unless you really want always-on-top behavior)
            SetWindowLongPtrW(_hwnd, GWL_EXSTYLE, _prev_ex_style | WS_EX_APPWINDOW);

            // Find target monitor (here: the one the window is currently mostly on)
            HMONITOR hmon = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoW(hmon, &mi);

            const RECT& target = mi.rcMonitor;  // full monitor area (including taskbar)

            // Move & resize — important: use SWP_FRAMECHANGED so non-client area updates
            SetWindowPos(_hwnd, HWND_TOP,          // or HWND_NOTOPMOST if you don't want to steal focus
                         target.left, target.top,
                         target.right - target.left,
                         target.bottom - target.top,
                         SWP_FRAMECHANGED | SWP_NOACTIVATE);  // NOACTIVATE = don't steal focus if you prefer

            // Optional dark mode refresh (some themes glitch otherwise)
            RedrawWindow(_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
        }
        else
        {
            // Going → windowed

            // Restore original styles
            SetWindowLongPtrW(_hwnd, GWL_STYLE,    _prev_style);
            SetWindowLongPtrW(_hwnd, GWL_EXSTYLE,  _prev_ex_style);

            // Restore position & size
            SetWindowPos(_hwnd, nullptr,
                         _prev_window_rect.left,
                         _prev_window_rect.top,
                         _prev_window_rect.right  - _prev_window_rect.left,
                         _prev_window_rect.bottom - _prev_window_rect.top,
                         SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);

            // If it was maximized before, maximize again
            if (_was_maximized)
                ShowWindow(_hwnd, SW_MAXIMIZE);
        }

        RECT client{};
        GetClientRect(_hwnd, &client);
        const uint32_t new_w{ static_cast<uint32_t>(client.right - client.left) };
        const uint32_t new_h{ static_cast<uint32_t>(client.bottom - client.top) };

        if (new_w != _width || new_h != _height)
        {
            _width  = new_w;
            _height = new_h;
            _on_window_resized.broadcast({ _width, _height });
        }
    }

    LRESULT win32_window_t::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
    {
        if (msg == WM_NCCREATE)
        {
            CREATESTRUCTW* cs{ reinterpret_cast<CREATESTRUCTW *>(lParam) };
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        if (win32_window_t* self{ reinterpret_cast<win32_window_t *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)) })
            return self->handle_message(msg, wParam, lParam);

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // PRIVATE
    LRESULT win32_window_t::handle_message(const UINT msg, const WPARAM wParam, const LPARAM lParam) noexcept
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
                _width  = LOWORD(lParam);
                _height = HIWORD(lParam);

                if (wParam == SIZE_MINIMIZED)
                {
                    _is_minimized = true;
                    return 0;
                }

                // For SIZE_RESTORED, SIZE_MAXIMIZED, etc.
                _is_minimized = false;

                if (_width == 0 || _height == 0)
                {
                    // Can sometimes get bogus 0x0 sizes during transitions;
                    // don't treat as minimized, but also don't spam resize.
                    return 0;
                }

                _on_window_resized.broadcast({ _width, _height });

                return 0;
            }
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            {
                const uint32_t vk{ static_cast<uint32_t>(wParam) };
                input::key_code key_code{ input::to_carrot_key(vk) };

                switch (vk)
                {
                    case VK_SHIFT:
                    {
                        const UINT scancode{ static_cast<UINT>(lParam >> 16 & 0xFF) }; // bits 16–23 = scan code
                        const UINT mapped_vk{ MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX) };

                        if (mapped_vk == VK_LSHIFT)
                        {
                            key_code = input::to_carrot_key(VK_LSHIFT);
                        }
                        else if (mapped_vk == VK_RSHIFT)
                        {
                            key_code = input::to_carrot_key(VK_RSHIFT);
                        }
                        break;
                    }
                    case VK_CONTROL:
                    {
                        const bool extended{ (lParam & 0x01000000) != 0 }; // bit 24
                        key_code = extended
                                       ? input::to_carrot_key(VK_RCONTROL)
                                       : input::to_carrot_key(VK_LCONTROL);
                        break;
                    }
                    case VK_MENU:
                    {
                        const bool extended{ (lParam & 0x01000000) != 0 };
                        key_code = extended
                                       ? input::to_carrot_key(VK_RMENU)
                                       : input::to_carrot_key(VK_LMENU);
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

                uint8_t mods = 0;

                if (GetKeyState(VK_SHIFT) & 0x8000) mods |= static_cast<uint8_t>(input::modifier::shift);
                if (GetKeyState(VK_CONTROL) & 0x8000) mods |= static_cast<uint8_t>(input::modifier::control);
                if (GetKeyState(VK_MENU) & 0x8000) mods |= static_cast<uint8_t>(input::modifier::alt);
                // Super / Win key — Windows sends VK_LWIN / VK_RWIN normally
                if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000)
                    mods |= static_cast<uint8_t>(input::modifier::super);

                e._mods = mods;

                _on_key.broadcast(e);
                break;
            }
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            {
                events::mouse_button_event_t e{ };
                e._button = msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP
                                ? input::mouse_button::left
                                : msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP
                                      ? input::mouse_button::right
                                      : input::mouse_button::middle;
                e._action = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN
                                ? events::key_action::press
                                : events::key_action::release;
                e._pos.x = LOWORD(lParam);
                e._pos.y = HIWORD(lParam);
                _on_mouse_button.broadcast(e);
                break;
            }
            case WM_MOUSEMOVE:
            {
                events::mouse_moved_event_t e{ };
                e._pos.x = LOWORD(lParam);
                e._pos.y = HIWORD(lParam);
                e._delta = e._pos - _last_mouse_position;
                _on_mouse_moved.broadcast(e);

                _last_mouse_position.x = LOWORD(lParam);
                _last_mouse_position.y = HIWORD(lParam);
                break;
            }
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            {
                events::mouse_scrolled_event_t e{ };
                const float delta{ static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) };

                if (msg == WM_MOUSEWHEEL)
                    e._delta.y = delta / static_cast<float>(WHEEL_DELTA);
                else
                    e._delta.x = delta / static_cast<float>(WHEEL_DELTA);

                _on_mouse_scrolled.broadcast(e);
                break;
            }
            default:
                break;
        }

        return DefWindowProcW(_hwnd, msg, wParam, lParam);
    }
} // namespace carrot::core::platform
