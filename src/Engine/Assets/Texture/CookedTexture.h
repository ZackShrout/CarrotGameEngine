//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/ImportedAssetCache.h"
#include "RHI/Texture.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace carrot::assets {
    struct cooked_texture_data_t
    {
        std::uint32_t cooked_format_version{ 1u };
        std::uint32_t importer_version{ 1u };
        std::uint32_t flags{ 0u };

        imported_asset_invalidation_t invalidation;

        std::uint32_t width{ 0u };
        std::uint32_t height{ 0u };
        std::uint32_t stride_bytes{ 0u };
        rhi::texture_format_t format{ rhi::texture_format_t::rgba8_unorm };
        std::vector<std::uint8_t> pixel_payload;
    };

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> serialize_cooked_texture(
        const cooked_texture_data_t& texture) noexcept;
    [[nodiscard]] std::optional<cooked_texture_data_t> deserialize_cooked_texture(
        std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] bool write_cooked_texture_file(const std::filesystem::path& path,
                                                 const cooked_texture_data_t& texture) noexcept;
    [[nodiscard]] std::optional<cooked_texture_data_t> load_cooked_texture_file(
        const std::filesystem::path& path) noexcept;
} // namespace carrot::assets
