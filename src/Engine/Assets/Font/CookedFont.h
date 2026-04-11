//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Texture.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace carrot::assets {
    struct cfont_invalidation_t
    {
        std::uint64_t source_font_content_hash{ 0u };
        std::uint64_t asset_definition_content_hash{ 0u };
        std::uint64_t import_settings_hash{ 0u };
        std::uint64_t reserved_hash{ 0u };
    };

    struct cfont_global_metrics_t
    {
        float em_size{ 0.0f };
        float line_height{ 0.0f };
        float ascent{ 0.0f };
        float descent{ 0.0f };
        float underline_position{ 0.0f };
        float underline_thickness{ 0.0f };

        std::uint32_t atlas_width{ 0u };
        std::uint32_t atlas_height{ 0u };
        rhi::texture_format_t atlas_format{ rhi::texture_format_t::rgba8_unorm };
        std::uint32_t atlas_channel_layout{ 0u };

        float msdf_pixel_range{ 0.0f };
        float distance_normalization{ 1.0f };
        float reserved_float0{ 0.0f };
        float reserved_float1{ 0.0f };
    };

    struct cfont_glyph_record_t
    {
        std::uint32_t codepoint{ 0u };
        std::uint32_t glyph_index{ 0u };
        float advance{ 0.0f };
        float plane_left{ 0.0f };
        float plane_top{ 0.0f };
        float plane_right{ 0.0f };
        float plane_bottom{ 0.0f };
        float uv_left{ 0.0f };
        float uv_top{ 0.0f };
        float uv_right{ 0.0f };
        float uv_bottom{ 0.0f };
    };

    struct cfont_kerning_pair_t
    {
        std::uint32_t left_codepoint{ 0u };
        std::uint32_t right_codepoint{ 0u };
        float adjustment{ 0.0f };
    };

    struct cooked_font_data_t
    {
        std::uint32_t cooked_format_version{ 1u };
        std::uint32_t importer_version{ 1u };
        std::uint32_t flags{ 0u };

        cfont_invalidation_t invalidation;
        cfont_global_metrics_t metrics;
        std::vector<cfont_glyph_record_t> glyphs;
        std::vector<cfont_kerning_pair_t> kerning_pairs;
        std::vector<std::uint8_t> atlas_payload;
    };

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> serialize_cooked_font(
        const cooked_font_data_t& font) noexcept;
    [[nodiscard]] std::optional<cooked_font_data_t> deserialize_cooked_font(
        std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] bool write_cooked_font_file(const std::filesystem::path& path,
                                              const cooked_font_data_t& font) noexcept;
    [[nodiscard]] std::optional<cooked_font_data_t> load_cooked_font_file(
        const std::filesystem::path& path) noexcept;
} // namespace carrot::assets
