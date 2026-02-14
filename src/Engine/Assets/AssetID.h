//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::assets {
    using asset_id_t = uint64_t;

    /**
     * @brief Generate a runtime asset ID from a stable string identifier.
     *
     * The hash must be:
     *  - deterministic
     *  - platform-independent
     *  - stable across runs
     */
    [[nodiscard]] inline asset_id_t make_asset_id(const std::string_view id)
    {
        constexpr uint64_t fnv_offset{ 14695981039346656037ull };
        constexpr uint64_t fnv_prime{ 1099511628211ull };

        uint64_t hash{ fnv_offset };
        for (const char c : id)
        {
            hash ^= static_cast<uint64_t>(c);
            hash *= fnv_prime;
        }
        return hash;
    }
} // carrot::assets
