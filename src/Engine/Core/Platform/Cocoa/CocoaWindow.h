//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Platform/Window.h"

#include <objc/objc.h>

namespace carrot::core::platform {
    class cocoa_window_t final : public window_t
    {
    public:
        explicit cocoa_window_t(uint32_t width, uint32_t height, std::string_view title) noexcept;
        ~cocoa_window_t() noexcept override;

        void poll_events() noexcept override;
        void set_should_close(bool should_close) noexcept override;

        [[nodiscard]] native_window_handle_t get_native_handle() const noexcept override;

        void set_mtk_view(void* mtk_view) noexcept { _mtk_view = mtk_view; }

    private:
        void* _controller{ nullptr };
        void* _mtk_view{ nullptr };
        id _ns_window{ nullptr };
        uint32_t _last_modifier_flags{ 0 };
        chlm::float2 _last_mouse_position{ 0.f, 0.f };
    };
} // nanamespace carrot::core::platform
