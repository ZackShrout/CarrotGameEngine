//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Platform/Window.h"

#include <wayland-client.h>

struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;

namespace carrot::core::platform {
    class wayland_window_t final : public window_t
    {
    public:
        explicit wayland_window_t(uint32_t width, uint32_t height, std::string_view title) noexcept;
        ~wayland_window_t() noexcept override;

        void poll_events() noexcept override;
        [[nodiscard]] bool should_close() const noexcept override { return _should_close; }

        [[nodiscard]] uint32_t get_width() const noexcept override { return _current_width; }
        [[nodiscard]] uint32_t get_height() const noexcept override { return _current_height; }

        [[nodiscard]] native_window_handle_t get_native_handle() const noexcept override;

        [[nodiscard]] wl_display* get_wl_display() const noexcept { return _display; }
        [[nodiscard]] wl_surface* get_wl_surface() const noexcept { return _surface; }
        [[nodiscard]] uint32_t get_current_width() const noexcept { return _current_width; }
        [[nodiscard]] uint32_t get_current_height() const noexcept { return _current_height; }
        [[nodiscard]] uint32_t get_pending_width() const noexcept { return _pending_width; }
        [[nodiscard]] uint32_t get_pending_height() const noexcept { return _pending_height; }
        [[nodiscard]] bool configure_pending() const noexcept { return _configure_pending; }

        void set_current_width(const uint32_t width) noexcept { _current_width = width; }
        void set_current_height(const uint32_t height) noexcept { _current_height = height; }
        void set_pending_width(const uint32_t width) noexcept { _pending_width = width; }
        void set_pending_height(const uint32_t height) noexcept { _pending_height = height; }
        void set_configure_pending(const bool configure) noexcept { _configure_pending = configure; }
        void set_should_close(const bool should_close) noexcept { _should_close = should_close; }

        // These two are only for the registry callback
        void set_compositor(wl_compositor* c) noexcept { _compositor = c; }
        void set_xdg_wm_base(xdg_wm_base* base) noexcept { _xdg_wm_base = base; }

    private:
        wl_display* _display{ nullptr };
        wl_compositor* _compositor{ nullptr };
        wl_surface* _surface{ nullptr };
        xdg_wm_base* _xdg_wm_base{ nullptr };
        xdg_surface* _xdg_surface{ nullptr };
        xdg_toplevel* _xdg_toplevel{ nullptr };

        uint32_t _current_width{ 1280 };
        uint32_t _current_height{ 720 };
        uint32_t _pending_width{ 0 };
        uint32_t _pending_height{ 0 };
        bool _should_close{ false };
        bool _configure_pending{ false };
    };
} // namespace carrot::core::platform
