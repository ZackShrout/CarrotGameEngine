//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string_view>

namespace carrot::input {
    struct input_action_id_t
    {
        std::uint32_t value{ 0u };

        [[nodiscard]] constexpr bool valid() const noexcept { return value != 0u; }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

        [[nodiscard]] constexpr bool operator==(const input_action_id_t& rhs) const noexcept = default;
    };

    struct input_action_id_hash_t
    {
        [[nodiscard]] constexpr std::size_t operator()(const input_action_id_t id) const noexcept
        {
            return static_cast<std::size_t>(id.value);
        }
    };

    namespace detail {
        [[nodiscard]] constexpr std::uint32_t fnv1a_32(const std::string_view text) noexcept
        {
            std::uint32_t hash{ 2166136261u };
            for (const char ch : text)
            {
                hash ^= static_cast<std::uint8_t>(ch);
                hash *= 16777619u;
            }

            return hash == 0u ? 1u : hash;
        }
    } // namespace detail

    [[nodiscard]] constexpr input_action_id_t make_input_action_id(const std::string_view authored_id) noexcept
    {
        return input_action_id_t{ .value = detail::fnv1a_32(authored_id) };
    }

    struct input_action_handle_t
    {
        input_action_id_t id{ };
        std::string_view authored_id{ };

        [[nodiscard]] constexpr bool valid() const noexcept { return id.valid() && !authored_id.empty(); }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
        [[nodiscard]] constexpr operator input_action_id_t() const noexcept { return id; }
    };

    [[nodiscard]] constexpr input_action_handle_t make_input_action(const std::string_view authored_id) noexcept
    {
        return input_action_handle_t{
            .id = make_input_action_id(authored_id),
            .authored_id = authored_id
        };
    }
} // namespace carrot::input
