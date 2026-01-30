//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "CocoaWindow.h"

namespace carrot::core::platform {
    cocoa_window_t::cocoa_window_t(const uint32_t width, const uint32_t height, std::string_view title) noexcept
    {
        _width = width;
        _height = height;

        LOG_CORE_INFO("Creating window with size {}x{}", width, height);
    }

    cocoa_window_t::~cocoa_window_t() noexcept
    {

    }

    void cocoa_window_t::poll_events() noexcept
    {

    }

    void cocoa_window_t::set_should_close(bool should_close) noexcept
    {

    }

    native_window_handle_t cocoa_window_t::get_native_handle() const noexcept
    {
        native_window_handle_t handle{ };

        handle.cocoa_t.ns_window = nullptr;
        handle.cocoa_t.metal_layer = nullptr;

        return handle;
    }
} // namespace carrot::core::plaform