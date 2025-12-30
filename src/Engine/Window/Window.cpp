//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "Window.h"

#include "Engine/Core/Platform/Wayland/WaylandWindow.h"

namespace carrot::window {
    static platform::wayland_window_t* g_primary_window{ nullptr };

    void create_primary_window(const uint32_t width, const uint32_t height, const char* title) noexcept
    {
        g_primary_window = new platform::wayland_window_t(width, height, title);
    }

    void destroy_primary_window() noexcept
    {
        delete g_primary_window;
        g_primary_window = nullptr;
    }

    void poll_events() noexcept
    {
        if (g_primary_window) g_primary_window->poll_events();
    }

    [[nodiscard]] platform::wayland_window_t& get_primary_window() noexcept
    {
        return *g_primary_window;
    }

    [[nodiscard]] bool should_close() noexcept
    {
        return g_primary_window ? g_primary_window->should_close() : true;
    }
} // namespace carrot::window
