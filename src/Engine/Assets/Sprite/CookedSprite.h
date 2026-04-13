//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/ImportedAssetCache.h"
#include "SpriteAsset.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace carrot::assets {
    struct cooked_sprite_data_t
    {
        std::uint32_t cooked_format_version{ 1u };
        std::uint32_t importer_version{ 1u };
        imported_asset_invalidation_t invalidation;
        sprite_asset_t sprite;
    };

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> serialize_cooked_sprite(
        const cooked_sprite_data_t& sprite) noexcept;
    [[nodiscard]] std::optional<cooked_sprite_data_t> deserialize_cooked_sprite(
        std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] bool write_cooked_sprite_file(const std::filesystem::path& path,
                                                const cooked_sprite_data_t& sprite) noexcept;
    [[nodiscard]] std::optional<cooked_sprite_data_t> load_cooked_sprite_file(
        const std::filesystem::path& path) noexcept;
} // namespace carrot::assets
