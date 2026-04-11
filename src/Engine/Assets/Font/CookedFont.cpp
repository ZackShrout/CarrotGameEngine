//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "CookedFont.h"

#include "Utils/File/FileUtils.h"

#include <bit>

namespace carrot::assets {
    namespace {
        constexpr std::array<std::uint8_t, 8> cfont_magic{
            'C', 'F', 'O', 'N', 'T', 0, 0, 0
        };
        constexpr std::uint32_t cfont_supported_version{ 1u };
        constexpr std::uint32_t cfont_alignment{ 16u };
        constexpr std::uint32_t cfont_channel_layout_msdf_rgb{ 1u };

        [[nodiscard]] constexpr size_t atlas_stride_bytes(const cfont_global_metrics_t& metrics) noexcept
        {
            switch (metrics.atlas_format)
            {
                case rhi::texture_format_t::rgba8_unorm:
                case rhi::texture_format_t::rgba8_srgb:
                    return 4u;
            }

            return 0u;
        }

        [[nodiscard]] bool validate_sorted_glyphs(const std::vector<cfont_glyph_record_t>& glyphs) noexcept
        {
            for (size_t i{ 1 }; i < glyphs.size(); ++i)
            {
                if (glyphs[i - 1].codepoint >= glyphs[i].codepoint)
                    return false;
            }

            return true;
        }

        [[nodiscard]] bool validate_sorted_kerning(const std::vector<cfont_kerning_pair_t>& kerning_pairs) noexcept
        {
            for (size_t i{ 1 }; i < kerning_pairs.size(); ++i)
            {
                const auto& prev{ kerning_pairs[i - 1] };
                const auto& curr{ kerning_pairs[i] };
                if (prev.left_codepoint > curr.left_codepoint)
                    return false;
                if (prev.left_codepoint == curr.left_codepoint && prev.right_codepoint >= curr.right_codepoint)
                    return false;
            }

            return true;
        }

        [[nodiscard]] bool validate_font(const cooked_font_data_t& font) noexcept
        {
            if (font.cooked_format_version != cfont_supported_version)
            {
                LOG_CORE_ERROR("Cooked font validation failed: unsupported format version {}", font.cooked_format_version);
                return false;
            }

            if (font.metrics.atlas_width == 0u || font.metrics.atlas_height == 0u)
            {
                LOG_CORE_ERROR("Cooked font validation failed: invalid atlas dimensions {}x{}",
                               font.metrics.atlas_width,
                               font.metrics.atlas_height);
                return false;
            }

            if (font.metrics.msdf_pixel_range <= 0.0f || font.metrics.distance_normalization <= 0.0f)
            {
                LOG_CORE_ERROR("Cooked font validation failed: invalid distance settings range={} normalization={}",
                               font.metrics.msdf_pixel_range,
                               font.metrics.distance_normalization);
                return false;
            }

            if (!validate_sorted_glyphs(font.glyphs) || !validate_sorted_kerning(font.kerning_pairs))
            {
                LOG_CORE_ERROR("Cooked font validation failed: glyph or kerning tables are not strictly sorted");
                return false;
            }

            const size_t stride{ atlas_stride_bytes(font.metrics) };
            if (stride == 0u)
            {
                LOG_CORE_ERROR("Cooked font validation failed: unsupported atlas format {}",
                               static_cast<std::uint32_t>(font.metrics.atlas_format));
                return false;
            }

            const size_t expected_payload_size{
                static_cast<size_t>(font.metrics.atlas_width) * static_cast<size_t>(font.metrics.atlas_height) * stride
            };

            if (font.atlas_payload.size() != expected_payload_size)
            {
                LOG_CORE_ERROR("Cooked font validation failed: atlas payload size {} does not match expected {}",
                               font.atlas_payload.size(),
                               expected_payload_size);
                return false;
            }

            return true;
        }

        [[nodiscard]] bool read_u32(std::span<const std::uint8_t> bytes,
                                    size_t offset,
                                    std::uint32_t& out_value) noexcept
        {
            if (offset + sizeof(std::uint32_t) > bytes.size())
                return false;

            out_value =
                (static_cast<std::uint32_t>(bytes[offset + 0]) << 0u) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
                (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
                (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
            return true;
        }

        [[nodiscard]] bool read_u64(std::span<const std::uint8_t> bytes,
                                    size_t offset,
                                    std::uint64_t& out_value) noexcept
        {
            if (offset + sizeof(std::uint64_t) > bytes.size())
                return false;

            out_value =
                (static_cast<std::uint64_t>(bytes[offset + 0]) << 0u) |
                (static_cast<std::uint64_t>(bytes[offset + 1]) << 8u) |
                (static_cast<std::uint64_t>(bytes[offset + 2]) << 16u) |
                (static_cast<std::uint64_t>(bytes[offset + 3]) << 24u) |
                (static_cast<std::uint64_t>(bytes[offset + 4]) << 32u) |
                (static_cast<std::uint64_t>(bytes[offset + 5]) << 40u) |
                (static_cast<std::uint64_t>(bytes[offset + 6]) << 48u) |
                (static_cast<std::uint64_t>(bytes[offset + 7]) << 56u);
            return true;
        }

        [[nodiscard]] bool read_f32(std::span<const std::uint8_t> bytes,
                                    size_t offset,
                                    float& out_value) noexcept
        {
            std::uint32_t bits{ 0u };
            if (!read_u32(bytes, offset, bits))
                return false;

            out_value = std::bit_cast<float>(bits);
            return true;
        }

        [[nodiscard]] bool validate_block(const size_t offset,
                                          const size_t size_bytes,
                                          const size_t total_size) noexcept
        {
            return offset <= total_size && size_bytes <= total_size - offset;
        }
    } // namespace

    std::optional<std::vector<std::uint8_t>> serialize_cooked_font(const cooked_font_data_t& font) noexcept
    {
        if (!validate_font(font))
            return std::nullopt;

        utils::file::binary_blob_writer_t writer;
        writer.reserve(128u + font.glyphs.size() * 48u + font.kerning_pairs.size() * 12u + font.atlas_payload.size());

        [[maybe_unused]] const size_t magic_offset{ writer.write_bytes(cfont_magic) };
        [[maybe_unused]] const size_t version_offset{ writer.write_u32(font.cooked_format_version) };
        [[maybe_unused]] const size_t importer_version_offset{ writer.write_u32(font.importer_version) };
        [[maybe_unused]] const size_t flags_offset{ writer.write_u32(font.flags) };

        const size_t invalidation_offset_offset{ writer.write_u32(0u) };
        [[maybe_unused]] const size_t invalidation_size_offset{ writer.write_u32(sizeof(cfont_invalidation_t)) };
        const size_t metrics_offset_offset{ writer.write_u32(0u) };
        [[maybe_unused]] const size_t metrics_size_offset{ writer.write_u32(64u) };
        const size_t glyph_table_offset_offset{ writer.write_u32(0u) };
        [[maybe_unused]] const size_t glyph_count_offset{ writer.write_u32(static_cast<std::uint32_t>(font.glyphs.size())) };
        const size_t kerning_table_offset_offset{ writer.write_u32(0u) };
        [[maybe_unused]] const size_t kerning_count_offset{
            writer.write_u32(static_cast<std::uint32_t>(font.kerning_pairs.size()))
        };
        const size_t atlas_payload_offset_offset{ writer.write_u32(0u) };
        [[maybe_unused]] const size_t atlas_payload_size_offset{
            writer.write_u32(static_cast<std::uint32_t>(font.atlas_payload.size()))
        };
        [[maybe_unused]] const size_t reserved0_offset{ writer.write_u32(0u) };
        [[maybe_unused]] const size_t reserved1_offset{ writer.write_u32(0u) };

        [[maybe_unused]] const size_t invalidation_aligned_offset{ writer.align(cfont_alignment) };

        const size_t invalidation_offset{ writer.size() };
        [[maybe_unused]] const size_t source_hash_offset{ writer.write_u64(font.invalidation.source_font_content_hash) };
        [[maybe_unused]] const size_t manifest_hash_offset{
            writer.write_u64(font.invalidation.asset_definition_content_hash)
        };
        [[maybe_unused]] const size_t settings_hash_offset{ writer.write_u64(font.invalidation.import_settings_hash) };
        [[maybe_unused]] const size_t reserved_hash_offset{ writer.write_u64(font.invalidation.reserved_hash) };

        [[maybe_unused]] const size_t metrics_aligned_offset{ writer.align(cfont_alignment) };

        const size_t metrics_offset{ writer.size() };
        [[maybe_unused]] const size_t em_size_offset{ writer.write_f32(font.metrics.em_size) };
        [[maybe_unused]] const size_t line_height_offset{ writer.write_f32(font.metrics.line_height) };
        [[maybe_unused]] const size_t ascent_offset{ writer.write_f32(font.metrics.ascent) };
        [[maybe_unused]] const size_t descent_offset{ writer.write_f32(font.metrics.descent) };
        [[maybe_unused]] const size_t underline_pos_offset{ writer.write_f32(font.metrics.underline_position) };
        [[maybe_unused]] const size_t underline_thickness_offset{ writer.write_f32(font.metrics.underline_thickness) };
        [[maybe_unused]] const size_t atlas_width_offset{ writer.write_u32(font.metrics.atlas_width) };
        [[maybe_unused]] const size_t atlas_height_offset{ writer.write_u32(font.metrics.atlas_height) };
        [[maybe_unused]] const size_t atlas_format_offset{
            writer.write_u32(static_cast<std::uint32_t>(font.metrics.atlas_format))
        };
        [[maybe_unused]] const size_t atlas_channel_layout_offset{ writer.write_u32(font.metrics.atlas_channel_layout) };
        [[maybe_unused]] const size_t msdf_range_offset{ writer.write_f32(font.metrics.msdf_pixel_range) };
        [[maybe_unused]] const size_t distance_norm_offset{ writer.write_f32(font.metrics.distance_normalization) };
        [[maybe_unused]] const size_t reserved_float0_offset{ writer.write_f32(font.metrics.reserved_float0) };
        [[maybe_unused]] const size_t reserved_float1_offset{ writer.write_f32(font.metrics.reserved_float1) };
        [[maybe_unused]] const size_t metrics_reserved0_offset{ writer.write_u32(0u) };
        [[maybe_unused]] const size_t metrics_reserved1_offset{ writer.write_u32(0u) };

        [[maybe_unused]] const size_t glyph_aligned_offset{ writer.align(cfont_alignment) };

        const size_t glyph_table_offset{ writer.size() };
        for (const cfont_glyph_record_t& glyph : font.glyphs)
        {
            [[maybe_unused]] const size_t glyph_codepoint_offset{ writer.write_u32(glyph.codepoint) };
            [[maybe_unused]] const size_t glyph_index_offset{ writer.write_u32(glyph.glyph_index) };
            [[maybe_unused]] const size_t glyph_advance_offset{ writer.write_f32(glyph.advance) };
            [[maybe_unused]] const size_t glyph_plane_left_offset{ writer.write_f32(glyph.plane_left) };
            [[maybe_unused]] const size_t glyph_plane_top_offset{ writer.write_f32(glyph.plane_top) };
            [[maybe_unused]] const size_t glyph_plane_right_offset{ writer.write_f32(glyph.plane_right) };
            [[maybe_unused]] const size_t glyph_plane_bottom_offset{ writer.write_f32(glyph.plane_bottom) };
            [[maybe_unused]] const size_t glyph_uv_left_offset{ writer.write_f32(glyph.uv_left) };
            [[maybe_unused]] const size_t glyph_uv_top_offset{ writer.write_f32(glyph.uv_top) };
            [[maybe_unused]] const size_t glyph_uv_right_offset{ writer.write_f32(glyph.uv_right) };
            [[maybe_unused]] const size_t glyph_uv_bottom_offset{ writer.write_f32(glyph.uv_bottom) };
        }

        [[maybe_unused]] const size_t kerning_aligned_offset{ writer.align(cfont_alignment) };

        const size_t kerning_table_offset{ writer.size() };
        for (const cfont_kerning_pair_t& pair : font.kerning_pairs)
        {
            [[maybe_unused]] const size_t left_codepoint_offset{ writer.write_u32(pair.left_codepoint) };
            [[maybe_unused]] const size_t right_codepoint_offset{ writer.write_u32(pair.right_codepoint) };
            [[maybe_unused]] const size_t kerning_adjustment_offset{ writer.write_f32(pair.adjustment) };
        }

        [[maybe_unused]] const size_t atlas_aligned_offset{ writer.align(cfont_alignment) };

        const size_t atlas_payload_offset{ writer.size() };
        [[maybe_unused]] const size_t atlas_data_offset{ writer.write_bytes(font.atlas_payload) };

        if (!writer.patch_u32(invalidation_offset_offset, static_cast<std::uint32_t>(invalidation_offset)) ||
            !writer.patch_u32(metrics_offset_offset, static_cast<std::uint32_t>(metrics_offset)) ||
            !writer.patch_u32(glyph_table_offset_offset, static_cast<std::uint32_t>(glyph_table_offset)) ||
            !writer.patch_u32(kerning_table_offset_offset, static_cast<std::uint32_t>(kerning_table_offset)) ||
            !writer.patch_u32(atlas_payload_offset_offset, static_cast<std::uint32_t>(atlas_payload_offset)))
        {
            return std::nullopt;
        }

        return std::move(writer).take();
    }

    std::optional<cooked_font_data_t> deserialize_cooked_font(const std::span<const std::uint8_t> bytes) noexcept
    {
        if (bytes.size() < 64u || !std::equal(cfont_magic.begin(), cfont_magic.end(), bytes.begin()))
            return std::nullopt;

        cooked_font_data_t font;

        size_t offset{ 8u };
        std::uint32_t invalidation_offset{ 0u };
        std::uint32_t invalidation_size{ 0u };
        std::uint32_t metrics_offset{ 0u };
        std::uint32_t metrics_size{ 0u };
        std::uint32_t glyph_table_offset{ 0u };
        std::uint32_t glyph_count{ 0u };
        std::uint32_t kerning_table_offset{ 0u };
        std::uint32_t kerning_count{ 0u };
        std::uint32_t atlas_payload_offset{ 0u };
        std::uint32_t atlas_payload_size{ 0u };

        if (!read_u32(bytes, offset, font.cooked_format_version)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, font.importer_version)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, font.flags)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, invalidation_offset)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, invalidation_size)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, metrics_offset)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, metrics_size)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, glyph_table_offset)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, glyph_count)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, kerning_table_offset)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, kerning_count)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, atlas_payload_offset)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, atlas_payload_size)) return std::nullopt; offset += 4u;

        if (font.cooked_format_version != cfont_supported_version ||
            invalidation_size != sizeof(cfont_invalidation_t) ||
            metrics_size != 64u)
        {
            return std::nullopt;
        }

        if (!validate_block(invalidation_offset, invalidation_size, bytes.size()) ||
            !validate_block(metrics_offset, metrics_size, bytes.size()) ||
            !validate_block(glyph_table_offset, static_cast<size_t>(glyph_count) * 44u, bytes.size()) ||
            !validate_block(kerning_table_offset, static_cast<size_t>(kerning_count) * 12u, bytes.size()) ||
            !validate_block(atlas_payload_offset, atlas_payload_size, bytes.size()))
        {
            return std::nullopt;
        }

        size_t invalidation_cursor{ invalidation_offset };
        if (!read_u64(bytes, invalidation_cursor, font.invalidation.source_font_content_hash)) return std::nullopt; invalidation_cursor += 8u;
        if (!read_u64(bytes, invalidation_cursor, font.invalidation.asset_definition_content_hash)) return std::nullopt; invalidation_cursor += 8u;
        if (!read_u64(bytes, invalidation_cursor, font.invalidation.import_settings_hash)) return std::nullopt; invalidation_cursor += 8u;
        if (!read_u64(bytes, invalidation_cursor, font.invalidation.reserved_hash)) return std::nullopt;

        size_t metrics_cursor{ metrics_offset };
        std::uint32_t atlas_format{ 0u };
        if (!read_f32(bytes, metrics_cursor, font.metrics.em_size)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.line_height)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.ascent)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.descent)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.underline_position)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.underline_thickness)) return std::nullopt; metrics_cursor += 4u;
        if (!read_u32(bytes, metrics_cursor, font.metrics.atlas_width)) return std::nullopt; metrics_cursor += 4u;
        if (!read_u32(bytes, metrics_cursor, font.metrics.atlas_height)) return std::nullopt; metrics_cursor += 4u;
        if (!read_u32(bytes, metrics_cursor, atlas_format)) return std::nullopt; metrics_cursor += 4u;
        if (!read_u32(bytes, metrics_cursor, font.metrics.atlas_channel_layout)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.msdf_pixel_range)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.distance_normalization)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.reserved_float0)) return std::nullopt; metrics_cursor += 4u;
        if (!read_f32(bytes, metrics_cursor, font.metrics.reserved_float1)) return std::nullopt;

        if (atlas_format > static_cast<std::uint32_t>(rhi::texture_format_t::rgba8_srgb))
            return std::nullopt;
        font.metrics.atlas_format = static_cast<rhi::texture_format_t>(atlas_format);

        font.glyphs.resize(glyph_count);
        size_t glyph_cursor{ glyph_table_offset };
        for (cfont_glyph_record_t& glyph : font.glyphs)
        {
            if (!read_u32(bytes, glyph_cursor, glyph.codepoint)) return std::nullopt; glyph_cursor += 4u;
            if (!read_u32(bytes, glyph_cursor, glyph.glyph_index)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.advance)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.plane_left)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.plane_top)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.plane_right)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.plane_bottom)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.uv_left)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.uv_top)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.uv_right)) return std::nullopt; glyph_cursor += 4u;
            if (!read_f32(bytes, glyph_cursor, glyph.uv_bottom)) return std::nullopt; glyph_cursor += 4u;
        }

        font.kerning_pairs.resize(kerning_count);
        size_t kerning_cursor{ kerning_table_offset };
        for (cfont_kerning_pair_t& pair : font.kerning_pairs)
        {
            if (!read_u32(bytes, kerning_cursor, pair.left_codepoint)) return std::nullopt; kerning_cursor += 4u;
            if (!read_u32(bytes, kerning_cursor, pair.right_codepoint)) return std::nullopt; kerning_cursor += 4u;
            if (!read_f32(bytes, kerning_cursor, pair.adjustment)) return std::nullopt; kerning_cursor += 4u;
        }

        font.atlas_payload.assign(bytes.begin() + atlas_payload_offset,
                                  bytes.begin() + atlas_payload_offset + atlas_payload_size);

        if (font.metrics.atlas_channel_layout != cfont_channel_layout_msdf_rgb || !validate_font(font))
            return std::nullopt;

        return font;
    }

    bool write_cooked_font_file(const std::filesystem::path& path, const cooked_font_data_t& font) noexcept
    {
        const auto serialized{ serialize_cooked_font(font) };
        return serialized.has_value() && utils::file::write_binary_file(path, *serialized);
    }

    std::optional<cooked_font_data_t> load_cooked_font_file(const std::filesystem::path& path) noexcept
    {
        const auto bytes{ utils::file::load_binary_file(path) };
        if (!bytes)
            return std::nullopt;

        return deserialize_cooked_font(*bytes);
    }
} // namespace carrot::assets
