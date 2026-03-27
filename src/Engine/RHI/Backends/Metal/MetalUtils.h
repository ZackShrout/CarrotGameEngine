//
// Created by Zack Shrout on 3/27/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace carrot::rhi::metal {
    // Carrot's shared shader/UV convention matches Vulkan/DX12 as currently authored.
    // For Metal CPU-side texture uploads, row order is flipped here so shared HLSL and
    // quad UV data can remain backend-agnostic.
    [[nodiscard]] inline std::vector<uint8_t> flip_rgba8_rows_for_metal(const void* data,
                                                                        const uint32_t width,
                                                                        const uint32_t height,
                                                                        const uint32_t bytes_per_row)
    {
        std::vector<uint8_t> flipped;
        if (!data || width == 0 || height == 0 || bytes_per_row == 0)
            return flipped;

        flipped.resize(static_cast<size_t>(bytes_per_row) * height);

        const auto* src = static_cast<const uint8_t*>(data);

        for (uint32_t y = 0; y < height; ++y)
        {
            const uint32_t src_y = height - 1 - y;

            std::memcpy(
                flipped.data() + static_cast<size_t>(y) * bytes_per_row,
                src + static_cast<size_t>(src_y) * bytes_per_row,
                bytes_per_row
            );
        }

        return flipped;
    }
} // namespace carrot::rhi::metal