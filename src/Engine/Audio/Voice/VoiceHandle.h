//
// Created by Zack Shrout on 2/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    struct voice_handle_t
    {
        uint32_t index{ 0 };
        uint32_t generation{ 0 };

        [[nodiscard]] constexpr bool is_valid() const noexcept
        {
            return generation != 0;
        }

        static constexpr voice_handle_t invalid() noexcept
        {
            return { };
        }
    };
}
