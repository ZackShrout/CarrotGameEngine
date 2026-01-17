//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Window.h"

#include "Core/Logger.h"
#include "Core/Platform/Wayland/WaylandWindow.h"

#include <memory>

namespace carrot::window {
    static std::unique_ptr<core::platform::window_t> g_primary_window{ nullptr };

    void create_primary_window(const uint32_t width, const uint32_t height, const std::string_view title) noexcept
    {
        switch (core::platform::current_platform())
        {
            case core::platform::platform_type::win32: break;
            case core::platform::platform_type::wayland:
                g_primary_window = std::make_unique<core::platform::wayland_window_t>(width, height, title.data());
                break;
            case core::platform::platform_type::cocoa: break;
            case core::platform::platform_type::unknown:
            default:
                LOG_CORE_FATAL("Could not create primary window - invalid platform.");
                break;
        }
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

    core::platform::native_window_handle_t get_native_handle() noexcept
    {
        return g_primary_window->get_native_handle();
    }
} // namespace carrot::window
