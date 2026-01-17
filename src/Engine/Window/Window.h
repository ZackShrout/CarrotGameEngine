//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Platform/Window.h"

namespace carrot::window {
    void create_primary_window(uint32_t width, uint32_t height, std::string_view title) noexcept;
    void destroy_primary_window() noexcept;

    void poll_events() noexcept;
    [[nodiscard]] bool should_close() noexcept;
    void set_should_close(bool should_close) noexcept;

    [[nodiscard]] core::platform::window_t&                 get_primary_window() noexcept;
    [[nodiscard]] uint32_t                                  get_width()  noexcept;
    [[nodiscard]] uint32_t                                  get_height() noexcept;
    [[nodiscard]] core::platform::native_window_handle_t    get_native_handle() noexcept;
} // namespace carrot::window
