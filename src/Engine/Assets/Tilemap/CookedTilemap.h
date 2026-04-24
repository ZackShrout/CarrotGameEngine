//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/ImportedAssetCache.h"
#include "TilemapAsset.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace carrot::assets {
    struct cooked_tilemap_data_t
    {
        std::uint32_t cooked_format_version{ 4u };
        std::uint32_t importer_version{ 1u };
        imported_asset_invalidation_t invalidation;
        tilemap_asset_t tilemap;
    };

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> serialize_cooked_tilemap(
        const cooked_tilemap_data_t& tilemap) noexcept;
    [[nodiscard]] std::optional<cooked_tilemap_data_t> deserialize_cooked_tilemap(
        std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] bool write_cooked_tilemap_file(const std::filesystem::path& path,
                                                 const cooked_tilemap_data_t& tilemap) noexcept;
    [[nodiscard]] std::optional<cooked_tilemap_data_t> load_cooked_tilemap_file(
        const std::filesystem::path& path) noexcept;
} // namespace carrot::assets
