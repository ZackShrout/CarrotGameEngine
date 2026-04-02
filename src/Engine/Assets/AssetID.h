//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace carrot::assets {
    using asset_id_t = uint64_t;

    /**
     * @brief Returns true if the logical asset ID is valid.
     *
     * Logical asset IDs are symbolic names, not file paths or URIs.
     *
     * Allowed characters:
     *  - lowercase letters: a-z
     *  - digits: 0-9
     *  - dot: .
     *  - underscore: _
     *
     * Additional rules:
     *  - must not be empty
     *  - must not begin or end with '.'
     *  - must not contain consecutive dots
     */
    [[nodiscard]] constexpr bool is_valid_logical_asset_id(const std::string_view id) noexcept
    {
        if (id.empty())
            return false;

        if (id.front() == '.' || id.back() == '.')
            return false;

        char prev{ '\0' };

        for (const char c : id)
        {
            const bool valid{
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '.' ||
                c == '_'
            };

            if (!valid)
                return false;

            if (c == '.' && prev == '.')
                return false;

            prev = c;
        }

        return true;
    }

    /**
     * @brief Produce a lowercase suggestion for an authored logical asset ID.
     *
     * This is intended for diagnostics and tooling only.
     * It does not guarantee the result is valid.
     */
    [[nodiscard]] constexpr  std::string recommend_logical_asset_id(const std::string_view id)
    {
        std::string out;
        out.reserve(id.size());

        bool last_was_dot{ false };

        for (char c : id)
        {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');

            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            {
                out.push_back(c);
                last_was_dot = false;
            }
            else if (c == '_')
            {
                out.push_back(c);
                last_was_dot = false;
            }
            else if (c == '.' || c == ' ' || c == '-' || c == '/' || c == '\\')
            {
                if (!out.empty() && !last_was_dot)
                {
                    out.push_back('.');
                    last_was_dot = true;
                }
            }
            else
            {
                // drop all other characters
            }
        }

        while (!out.empty() && out.front() == '.')
            out.erase(out.begin());

        while (!out.empty() && out.back() == '.')
            out.pop_back();

        return out;
    }

    /**
     * @brief Generate a runtime asset ID from a stable string identifier.
     *
     * The hash must be:
     *  - deterministic
     *  - platform-independent
     *  - stable across runs
     */
    [[nodiscard]] constexpr asset_id_t make_asset_id(const std::string_view id)
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
