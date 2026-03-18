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
        void set_fullscreen(bool fullscreen) noexcept override;

        [[nodiscard]] native_window_handle_t get_native_handle() const noexcept override;

        void set_metal_layer(void* metal_layer) noexcept { _metal_layer = metal_layer; }
        void set_fullscreen_state(const bool fullscreen) noexcept {_is_fullscreen = fullscreen;}
        void update_size(uint32_t width, uint32_t height) noexcept;

    private:
        void* _controller{ nullptr };
        void* _metal_layer{ nullptr };
        id _ns_window{ nullptr };
        uint32_t _last_modifier_flags{ 0 };
        chlm::float2 _last_mouse_position{ 0.f, 0.f };
    };
} // namespace carrot::core::platform
