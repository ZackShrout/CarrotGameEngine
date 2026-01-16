//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

#ifdef CARROT_PLATFORM_WAYLAND
struct wl_display;
struct wl_surface;
#elifdef CARROT_PLATFORM_WIN32
using HWND = void *;
using HINSTANCE = void *;
#elifdef CARROT_PLATFORM_COCOA
using NSWindow = void *;
using CAMetalLayer = void *;
#endif // #ifdef CARROT_PLATFORM_WAYLAND

namespace carrot::core::platform {
    enum class platform_type : std::uint8_t
    {
        unknown,
        wayland,
        win32,
        cocoa,
        // future: Xcb, Headless, ...
    };

    [[nodiscard]] inline platform_type current_platform() noexcept
    {
#if defined(CARROT_PLATFORM_WAYLAND)
        return platform_type::wayland;
#elif defined(CARROT_PLATFORM_WIN32)
        return platform_type::win32;
#elif defined(CARROT_PLATFORM_COCOA)
        return platform_type::cocoa;
#else
        return platform_type::unknown;
#endif
    }

    // Very thin, cheap union-like handle
    union native_window_handle_t
    {
        void* any{ nullptr }; // for generic passing
#if defined(CARROT_PLATFORM_WAYLAND)
        struct
        {
            wl_display* display;
            wl_surface* surface;
        } wayland_t;
#elif defined(CARROT_PLATFORM_WIN32)
        struct
        {
            HWND hwnd;
            HINSTANCE hinstance;
        } win32_t;
#elif defined(CARROT_PLATFORM_COCOA)
        struct
        {
            NSWindow* ns_window;
            CAMetalLayer* metal_layer; // usually needed for Metal
        } cocoa_t;
#endif
    };
} // namespace carrot::core::platform
