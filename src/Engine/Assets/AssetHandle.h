//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::assets {
    struct asset_handle_t
    {
        uint32_t index{ 0 };
        uint32_t generation{ 0 };

        explicit operator bool() const noexcept { return generation != 0; }
    };
} // namespace carrot::assets
