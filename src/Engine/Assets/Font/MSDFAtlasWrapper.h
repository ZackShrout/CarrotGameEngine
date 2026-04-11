//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "CookedFont.h"
#include "FontAsset.h"

#include <optional>
#include <span>

namespace carrot::assets {
    [[nodiscard]] std::optional<cooked_font_data_t> generate_cooked_font_with_msdf_atlas(
        const font_asset_record_t& record,
        std::span<const std::uint8_t> source_bytes,
        std::span<const std::uint32_t> codepoints,
        std::uint64_t source_hash,
        std::uint64_t manifest_hash,
        std::uint32_t importer_version) noexcept;
}
