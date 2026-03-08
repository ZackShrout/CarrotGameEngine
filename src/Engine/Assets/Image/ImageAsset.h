//
// Created by zshrout on 3/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <vector>

namespace carrot::assets {
    struct image_rgba8_t
    {
        uint32_t width{ 0 };
        uint32_t height{ 0 };
        uint32_t stride_bytes{ 0 };
        bool is_srgb{ true };
        std::vector<uint8_t> pixels;

        [[nodiscard]] bool valid() const noexcept
        {
            return width > 0 && height > 0 && !pixels.empty();
        }

        [[nodiscard]] size_t size_bytes() const noexcept
        {
            return pixels.size();
        }

        [[nodiscard]] const uint8_t* data() const noexcept
        {
            return pixels.data();
        }

        [[nodiscard]] uint8_t* data() noexcept
        {
            return pixels.data();
        }
    };
} // carrot::assets
