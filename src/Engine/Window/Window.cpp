//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Window.h"

#include <algorithm>
#include <unordered_map>

#ifdef CARROT_PLATFORM_WIN32
#include "Core/Platform/Win32/Win32Window.h"
#elif defined(CARROT_PLATFORM_WAYLAND)
#include "Core/Platform/Wayland/WaylandWindow.h"
#elif defined(CARROT_PLATFORM_COCOA)
#include "Core/Platform/Cocoa/CocoaWindow.h"
#endif

namespace carrot::window {
    namespace {
        struct window_manager_state_t
        {
            std::unordered_map<window_id_t, std::unique_ptr<core::platform::window_t>> windows;
            std::vector<window_id_t> window_order;
            window_id_t main_window_id{ invalid_window_id };
            window_id_t next_window_id{ 1 };
        };

        window_manager_state_t g_state;

        [[nodiscard]] std::unique_ptr<core::platform::window_t> create_platform_window(const uint32_t width,
                                                                                         const uint32_t height,
                                                                                         const std::string_view title) noexcept
        {
#ifdef CARROT_PLATFORM_WIN32
            return std::make_unique<core::platform::win32_window_t>(width, height, title.data());
#elif defined(CARROT_PLATFORM_WAYLAND)
            return std::make_unique<core::platform::wayland_window_t>(width, height, title.data());
#elif defined(CARROT_PLATFORM_COCOA)
            return std::make_unique<core::platform::cocoa_window_t>(width, height, title.data());
#else
            LOG_CORE_FATAL("Could not create window - invalid platform.");
            return nullptr;
#endif
        }
    } // anonymous namespace

    window_id_t create_window(const window_create_desc_t& desc) noexcept
    {
        std::unique_ptr<core::platform::window_t> window{ create_platform_window(desc.width, desc.height, desc.title) };
        if (!window)
            return invalid_window_id;

        const window_id_t id{ g_state.next_window_id++ };
        g_state.window_order.push_back(id);
        g_state.windows.emplace(id, std::move(window));

        if (g_state.main_window_id == invalid_window_id)
            g_state.main_window_id = id;

        return id;
    }

    bool destroy_window(const window_id_t id) noexcept
    {
        const auto it{ g_state.windows.find(id) };
        if (it == g_state.windows.end())
            return false;

        g_state.windows.erase(it);
        std::erase(g_state.window_order, id);

        if (g_state.main_window_id == id)
        {
            g_state.main_window_id = g_state.window_order.empty()
                                         ? invalid_window_id
                                         : g_state.window_order.front();
        }

        return true;
    }

    core::platform::window_t* get_window(const window_id_t id) noexcept
    {
        const auto it{ g_state.windows.find(id) };
        return it != g_state.windows.end() ? it->second.get() : nullptr;
    }

    std::vector<window_id_t> get_window_ids() noexcept
    {
        return g_state.window_order;
    }

    size_t get_window_count() noexcept
    {
        return g_state.windows.size();
    }

    void destroy_all_windows() noexcept
    {
        g_state.windows.clear();
        g_state.window_order.clear();
        g_state.main_window_id = invalid_window_id;
    }

    bool has_window(const window_id_t id) noexcept
    {
        return g_state.windows.contains(id);
    }

    window_id_t get_main_window_id() noexcept
    {
        return g_state.main_window_id;
    }

    bool set_main_window(const window_id_t id) noexcept
    {
        if (id == invalid_window_id)
            return false;

        if (!g_state.windows.contains(id))
            return false;

        g_state.main_window_id = id;
        return true;
    }

    void create_primary_window(const uint32_t width, const uint32_t height, const std::string_view title) noexcept
    {
        if (g_state.main_window_id != invalid_window_id)
            return;

        (void)create_window(window_create_desc_t{
            .width = width,
            .height = height,
            .title = title
        });
    }

    void destroy_primary_window() noexcept
    {
        if (g_state.main_window_id == invalid_window_id)
            return;

        (void)destroy_window(g_state.main_window_id);
    }

    void poll_events() noexcept
    {
        const std::vector<window_id_t> window_ids{ g_state.window_order };
#ifdef CARROT_PLATFORM_WAYLAND
        core::platform::wayland_window_t::begin_poll_cycle();
#endif
        for (const window_id_t id : window_ids)
        {
            core::platform::window_t* window{ get_window(id) };
            if (window)
                window->poll_events();
        }
    }

    [[nodiscard]] bool should_close() noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        return main_window ? main_window->should_close() : true;
    }

    [[nodiscard]] bool should_close(const window_id_t id) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        return window ? window->should_close() : true;
    }

    void set_should_close(const bool should_close) noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        if (main_window)
            main_window->set_should_close(should_close);
    }

    void set_should_close(const window_id_t id, const bool should_close) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        if (window)
            window->set_should_close(should_close);
    }

    [[nodiscard]] core::platform::window_t& get_primary_window() noexcept
    {
        return *g_state.windows.at(g_state.main_window_id);
    }

    uint32_t get_width() noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        return main_window ? main_window->get_width() : 0u;
    }

    uint32_t get_width(const window_id_t id) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        return window ? window->get_width() : 0u;
    }

    uint32_t get_height() noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        return main_window ? main_window->get_height() : 0u;
    }

    uint32_t get_height(const window_id_t id) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        return window ? window->get_height() : 0u;
    }

    bool is_minimized() noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        return main_window ? main_window->is_minimized() : true;
    }

    bool is_minimized(const window_id_t id) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        return window ? window->is_minimized() : true;
    }

    core::platform::native_window_handle_t get_native_handle() noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        return main_window ? main_window->get_native_handle() : core::platform::native_window_handle_t{ };
    }

    core::platform::native_window_handle_t get_native_handle(const window_id_t id) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        return window ? window->get_native_handle() : core::platform::native_window_handle_t{ };
    }

    bool is_fullscreen() noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        return main_window ? main_window->is_fullscreen() : false;
    }

    bool is_fullscreen(const window_id_t id) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        return window ? window->is_fullscreen() : false;
    }

    void set_fullscreen(const bool fullscreen) noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        if (main_window)
            main_window->set_fullscreen(fullscreen);
    }

    void set_fullscreen(const window_id_t id, const bool fullscreen) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        if (window)
            window->set_fullscreen(fullscreen);
    }

    void request_focus() noexcept
    {
        core::platform::window_t* main_window{ get_window(g_state.main_window_id) };
        if (main_window)
            main_window->request_focus();
    }

    void request_focus(const window_id_t id) noexcept
    {
        core::platform::window_t* window{ get_window(id) };
        if (window)
            window->request_focus();
    }
} // namespace carrot::window
