//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Platform.h"
#include <string_view>

namespace carrot::core::platform {

    class window_t
    {
    public:
        virtual ~window_t() = default;

        virtual void poll_events() noexcept = 0;
        [[nodiscard]] virtual bool should_close() const noexcept = 0;

        [[nodiscard]] virtual uint32_t get_width()  const noexcept = 0;
        [[nodiscard]] virtual uint32_t get_height() const noexcept = 0;

        [[nodiscard]] virtual native_window_handle_t get_native_handle() const noexcept = 0;
    };

} // namespace carrot::core::platform