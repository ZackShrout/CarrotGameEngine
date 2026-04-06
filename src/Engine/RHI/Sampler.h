//
// Created by Zack Shrout on 3/27/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace carrot::rhi {
    enum class sampler_filter_t : std::uint8_t
    {
        nearest,
        linear
    };

    enum class sampler_mip_filter_t : std::uint8_t
    {
        nearest,
        linear
    };

    enum class sampler_address_mode_t : std::uint8_t
    {
        clamp_to_edge,
        repeat,
        mirrored_repeat
    };

    struct sampler_desc_t
    {
        sampler_filter_t min_filter{ sampler_filter_t::linear };
        sampler_filter_t mag_filter{ sampler_filter_t::linear };
        sampler_mip_filter_t mip_filter{ sampler_mip_filter_t::linear };

        sampler_address_mode_t address_u{ sampler_address_mode_t::clamp_to_edge };
        sampler_address_mode_t address_v{ sampler_address_mode_t::clamp_to_edge };
        sampler_address_mode_t address_w{ sampler_address_mode_t::clamp_to_edge };

        float mip_lod_bias{ 0.f };
        float min_lod{ 0.f };
        float max_lod{ 32.f };

        [[nodiscard]] bool operator==(const sampler_desc_t& other) const noexcept
        {
            return min_filter == other.min_filter
                && mag_filter == other.mag_filter
                && mip_filter == other.mip_filter
                && address_u == other.address_u
                && address_v == other.address_v
                && address_w == other.address_w
                && std::bit_cast<std::uint32_t>(mip_lod_bias) == std::bit_cast<std::uint32_t>(other.mip_lod_bias)
                && std::bit_cast<std::uint32_t>(min_lod) == std::bit_cast<std::uint32_t>(other.min_lod)
                && std::bit_cast<std::uint32_t>(max_lod) == std::bit_cast<std::uint32_t>(other.max_lod);
        }
    };

    class rhi_sampler_t
    {
    public:
        explicit rhi_sampler_t(sampler_desc_t desc)
            : _desc{ std::move(desc) } {}

        virtual ~rhi_sampler_t() = default;

        [[nodiscard]] const sampler_desc_t& desc() const noexcept { return _desc; }

    private:
        sampler_desc_t _desc{ };
    };

    struct sampler_desc_hash_t
    {
        [[nodiscard]] std::size_t operator()(const sampler_desc_t& desc) const noexcept
        {
            std::size_t seed{ 0 };

            const auto hash_combine = [&seed]<typename T>(const T& value) noexcept {
                const std::size_t hash{ std::hash<T>{ }(value) };
                seed ^= hash + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
            };

            hash_combine(static_cast<std::uint8_t>(desc.min_filter));
            hash_combine(static_cast<std::uint8_t>(desc.mag_filter));
            hash_combine(static_cast<std::uint8_t>(desc.mip_filter));
            hash_combine(static_cast<std::uint8_t>(desc.address_u));
            hash_combine(static_cast<std::uint8_t>(desc.address_v));
            hash_combine(static_cast<std::uint8_t>(desc.address_w));
            hash_combine(std::bit_cast<std::uint32_t>(desc.mip_lod_bias));
            hash_combine(std::bit_cast<std::uint32_t>(desc.min_lod));
            hash_combine(std::bit_cast<std::uint32_t>(desc.max_lod));

            return seed;
        }
    };
} // namespace carrot::rhi
