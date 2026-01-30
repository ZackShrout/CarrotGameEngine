//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Window.h"

#include "Core/Logger.h"

#include <memory>

#include "Core/Platform/Cocoa/CocoaWindow.h"

#ifdef CARROT_PLATFORM_WIN32
#include "Core/Platform/Win32/Win32Window.h"
#elifdef CARROT_PLATFORM_WAYLAND
#include "Core/Platform/Wayland/WaylandWindow.h"
#elifdef CARROT_PLATFORM_COCOA
#include "Core/Platform/Cocoa/CocoaWindow.h"
#endif

namespace carrot::window {
    static std::unique_ptr<core::platform::window_t> g_primary_window{ nullptr };

    void create_primary_window(const uint32_t width, const uint32_t height, const std::string_view title) noexcept
    {
#ifdef CARROT_PLATFORM_WIN32
        g_primary_window = std::make_unique<core::platform::win32_window_t>(width, height, title.data());
#elifdef CARROT_PLATFORM_WAYLAND
        g_primary_window = std::make_unique<core::platform::wayland_window_t>(width, height, title.data());
#elifdef CARROT_PLATFORM_COCOA
        g_primary_window = std::make_unique<core::platform::cocoa_window_t>(width, height, title.data());
#else
        LOG_CORE_FATAL("Could not create primary window - invalid platform.");
#endif
    }

    void destroy_primary_window() noexcept
    {
        g_primary_window.reset();
    }

    void poll_events() noexcept
    {
        if (g_primary_window) g_primary_window->poll_events();
    }

    [[nodiscard]] bool should_close() noexcept
    {
        return g_primary_window ? g_primary_window->should_close() : true;
    }

    void set_should_close(const bool should_close) noexcept
    {
        if (g_primary_window)
            g_primary_window->set_should_close(should_close);
    }

    [[nodiscard]] core::platform::window_t& get_primary_window() noexcept
    {
        return *g_primary_window;
    }

    uint32_t get_width() noexcept
    {
        return g_primary_window->get_width();
    }

    uint32_t get_height() noexcept
    {
        return g_primary_window->get_height();
    }

    bool is_minimized() noexcept
    {
        return g_primary_window->is_minimized();
    }

    core::platform::native_window_handle_t get_native_handle() noexcept
    {
        return g_primary_window->get_native_handle();
    }

    bool is_fullscreen() noexcept
    {
        return g_primary_window->is_fullscreen();
    }

    void set_fullscreen(const bool fullscreen) noexcept
    {
        g_primary_window->set_fullscreen(fullscreen);
    }
} // namespace carrot::window
