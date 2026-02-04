//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

#ifdef CARROT_PLATFORM_WAYLAND
struct wl_display;
struct wl_surface;
#endif // #ifdef CARROT_PLATFORM_WAYLAND

namespace carrot::core::platform {
    enum class platform_type : std::uint8_t
    {
        win32,
        wayland,
        cocoa,
        unknown,
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
            void* hwnd; // HWND
            void* hinstance; // HINSTANCE
        } win32_t;
#elif defined(CARROT_PLATFORM_COCOA)
        struct
        {
            void* ns_window; // NSWindow*
            void* metal_layer; // CAMetalLayer*
        } cocoa_t;
#endif
    };
} // namespace carrot::core::platform
