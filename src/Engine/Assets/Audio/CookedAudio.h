//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/ImportedAssetCache.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace carrot::assets {
    struct cooked_audio_data_t
    {
        std::uint32_t cooked_format_version{ 1u };
        std::uint32_t importer_version{ 1u };
        std::uint32_t flags{ 0u };

        imported_asset_invalidation_t invalidation;

        std::uint32_t sample_rate{ 0u };
        std::uint32_t channels{ 0u };
        std::uint32_t frame_count{ 0u };
        std::vector<float> pcm_payload;
    };

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> serialize_cooked_audio(
        const cooked_audio_data_t& audio) noexcept;
    [[nodiscard]] std::optional<cooked_audio_data_t> deserialize_cooked_audio(
        std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] bool write_cooked_audio_file(const std::filesystem::path& path,
                                               const cooked_audio_data_t& audio) noexcept;
    [[nodiscard]] std::optional<cooked_audio_data_t> load_cooked_audio_file(
        const std::filesystem::path& path) noexcept;
} // namespace carrot::assets
