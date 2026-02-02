//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Platform/Window.h"

namespace carrot::core::platform {
    class cocoa_window_t final : public window_t
    {
    public:
        explicit cocoa_window_t(uint32_t width, uint32_t height, std::string_view title) noexcept;
        ~cocoa_window_t() noexcept override;

        void poll_events() noexcept override;
        void set_should_close(bool should_close) noexcept override;

        [[nodiscard]] native_window_handle_t get_native_handle() const noexcept override;

    private:
        void* _controller{ nullptr };
    };
} // nanamespace carrot::core::platform
