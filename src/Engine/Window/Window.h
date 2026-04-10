//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Platform/Window.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace carrot::window {
    using window_id_t = uint64_t;
    constexpr window_id_t invalid_window_id{ 0 };

    struct window_create_desc_t
    {
        uint32_t width{ 1280 };
        uint32_t height{ 720 };
        std::string_view title{ "Carrot Window" };
    };

    [[nodiscard]] window_id_t create_window(const window_create_desc_t& desc) noexcept;
    [[nodiscard]] bool destroy_window(window_id_t id) noexcept;
    [[nodiscard]] core::platform::window_t* get_window(window_id_t id) noexcept;
    [[nodiscard]] std::vector<window_id_t> get_window_ids() noexcept;
    [[nodiscard]] size_t get_window_count() noexcept;
    void destroy_all_windows() noexcept;
    [[nodiscard]] bool has_window(window_id_t id) noexcept;

    [[nodiscard]] window_id_t get_main_window_id() noexcept;
    [[nodiscard]] bool set_main_window(window_id_t id) noexcept;

    void create_primary_window(uint32_t width, uint32_t height, std::string_view title) noexcept;
    void destroy_primary_window() noexcept;

    void poll_events() noexcept;
    [[nodiscard]] bool should_close() noexcept;
    void set_should_close(bool should_close) noexcept;

    [[nodiscard]] core::platform::window_t&                 get_primary_window() noexcept;
    [[nodiscard]] uint32_t                                  get_width()  noexcept;
    [[nodiscard]] uint32_t                                  get_height() noexcept;
    [[nodiscard]] bool                                      is_minimized() noexcept;
    [[nodiscard]] bool                                      is_resizing() noexcept;
    [[nodiscard]] core::platform::native_window_handle_t    get_native_handle() noexcept;
    [[nodiscard]] bool                                      should_close(window_id_t id) noexcept;
    void                                                    set_should_close(window_id_t id, bool should_close) noexcept;
    [[nodiscard]] uint32_t                                  get_width(window_id_t id) noexcept;
    [[nodiscard]] uint32_t                                  get_height(window_id_t id) noexcept;
    [[nodiscard]] bool                                      is_minimized(window_id_t id) noexcept;
    [[nodiscard]] bool                                      is_resizing(window_id_t id) noexcept;
    [[nodiscard]] core::platform::native_window_handle_t    get_native_handle(window_id_t id) noexcept;

    [[nodiscard]] bool                                      is_fullscreen() noexcept;
    void                                                    set_fullscreen(bool fullscreen) noexcept;
    [[nodiscard]] bool                                      is_fullscreen(window_id_t id) noexcept;
    void                                                    set_fullscreen(window_id_t id, bool fullscreen) noexcept;
    void                                                    request_focus() noexcept;
    void                                                    request_focus(window_id_t id) noexcept;
} // namespace carrot::window
