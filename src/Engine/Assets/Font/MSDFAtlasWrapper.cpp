//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MSDFAtlasWrapper.h"

#include <array>
#include <cmath>
#include <unordered_map>
#include <msdf-atlas-gen/msdf-atlas-gen.h>

namespace carrot::assets {
    namespace {
        constexpr double k_desired_em_pixels{ 32.0 };
        constexpr double k_angle_threshold_radians{ 3.0 };
        constexpr double k_miter_limit{ 1.0 };
        struct freetype_guard_t
        {
            msdfgen::FreetypeHandle* library{ nullptr };

            freetype_guard_t() noexcept
                : library{ msdfgen::initializeFreetype() } {}

            ~freetype_guard_t()
            {
                if (library != nullptr)
                    msdfgen::deinitializeFreetype(library);
            }

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return library != nullptr;
            }
        };

        struct font_guard_t
        {
            msdfgen::FontHandle* font{ nullptr };

            ~font_guard_t()
            {
                if (font != nullptr)
                    msdfgen::destroyFont(font);
            }
        };

    }

    std::optional<cooked_font_data_t> generate_cooked_font_with_msdf_atlas(
        const font_asset_record_t& record,
        const std::span<const std::uint8_t> source_bytes,
        const std::span<const std::uint32_t> codepoints,
        const std::uint64_t source_hash,
        const std::uint64_t manifest_hash,
        const std::uint32_t importer_version) noexcept
    {
        freetype_guard_t freetype{};
        if (!freetype)
        {
            LOG_ASSET_ERROR("MSDF font generation failed for '{}': could not initialize FreeType", record.logical_id);
            return std::nullopt;
        }

        font_guard_t font{
            .font = msdfgen::loadFontData(freetype.library,
                                          source_bytes.data(),
                                          static_cast<int>(source_bytes.size()))
        };
        if (font.font == nullptr)
        {
            LOG_ASSET_ERROR("MSDF font generation failed for '{}': loadFontData returned null", record.logical_id);
            return std::nullopt;
        }

        constexpr double font_scale{ 1.0 };

        msdf_atlas::Charset charset{};
        for (const std::uint32_t codepoint : codepoints)
            charset.add(codepoint);

        std::vector<msdf_atlas::GlyphGeometry> glyphs;
        msdf_atlas::FontGeometry font_geometry{ &glyphs };
        if (font_geometry.loadCharset(font.font,
                                      font_scale,
                                      charset,
                                      true,
                                      record.msdf.include_kerning) < 0)
        {
            LOG_ASSET_ERROR("MSDF font generation failed for '{}': loadCharset returned an error", record.logical_id);
            return std::nullopt;
        }

        if (glyphs.empty())
        {
            LOG_ASSET_ERROR("MSDF font generation failed for '{}': loadCharset produced no glyphs", record.logical_id);
            return std::nullopt;
        }

        for (msdf_atlas::GlyphGeometry& glyph : glyphs)
        {
            if (!glyph.isWhitespace())
                glyph.edgeColoring(&msdfgen::edgeColoringInkTrap,
                                   k_angle_threshold_radians,
                                   static_cast<unsigned long long>(glyph.getCodepoint()));
        }

        msdf_atlas::TightAtlasPacker atlas_packer{};
        atlas_packer.setDimensions(static_cast<int>(record.msdf.atlas_width),
                                   static_cast<int>(record.msdf.atlas_height));
        atlas_packer.setScale(k_desired_em_pixels);
        atlas_packer.setPixelRange(record.msdf.pixel_range);
        atlas_packer.setMiterLimit(k_miter_limit);
        atlas_packer.setSpacing(0);
        if (const int remaining{ atlas_packer.pack(glyphs.data(), static_cast<int>(glyphs.size())) };
            remaining != 0)
        {
            LOG_ASSET_ERROR("MSDF font generation failed for '{}': atlas pack left {} glyph(s) unpacked",
                            record.logical_id,
                            remaining);
            return std::nullopt;
        }

        int atlas_width{ 0 };
        int atlas_height{ 0 };
        atlas_packer.getDimensions(atlas_width, atlas_height);
        if (atlas_width <= 0 || atlas_height <= 0)
        {
            LOG_ASSET_ERROR("MSDF font generation failed for '{}': atlas dimensions were {}x{}",
                            record.logical_id,
                            atlas_width,
                            atlas_height);
            return std::nullopt;
        }

        const double atlas_scale{ atlas_packer.getScale() };
        if (atlas_scale <= 0.0)
        {
            LOG_ASSET_ERROR("MSDF font generation failed for '{}': atlas packer produced non-positive scale {}",
                            record.logical_id,
                            atlas_scale);
            return std::nullopt;
        }

        msdf_atlas::GeneratorAttributes generator_attributes{};
        generator_attributes.scanlinePass = true;

        using atlas_generator_t =
            msdf_atlas::ImmediateAtlasGenerator<float,
                                                4,
                                                msdf_atlas::mtsdfGenerator,
                                                msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;

        atlas_generator_t generator{ atlas_width, atlas_height };
        generator.setAttributes(generator_attributes);
        generator.setThreadCount(1);
        generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

        msdfgen::BitmapConstSection<msdf_atlas::byte, 4> atlas_bitmap =
            (msdfgen::BitmapConstSection<msdf_atlas::byte, 4>) generator.atlasStorage();

        if (atlas_bitmap.width <= 0 || atlas_bitmap.height <= 0 || atlas_bitmap.pixels == nullptr)
        {
            LOG_ASSET_ERROR("MSDF font generation failed for '{}': atlas generator returned invalid bitmap {}x{}",
                            record.logical_id,
                            atlas_bitmap.width,
                            atlas_bitmap.height);
            return std::nullopt;
        }

        atlas_bitmap.reorient(msdfgen::Y_DOWNWARD);

        cooked_font_data_t cooked{};
        cooked.importer_version = importer_version;
        cooked.invalidation.source_font_content_hash = source_hash;
        cooked.invalidation.asset_definition_content_hash = manifest_hash;
        cooked.invalidation.import_settings_hash = 0u;

        const msdfgen::FontMetrics& metrics{ font_geometry.getMetrics() };
        cooked.metrics.em_size = static_cast<float>(metrics.emSize);
        cooked.metrics.line_height = static_cast<float>(metrics.lineHeight);
        cooked.metrics.ascent = static_cast<float>(metrics.ascenderY);
        cooked.metrics.descent = static_cast<float>(metrics.descenderY);
        cooked.metrics.underline_position = static_cast<float>(metrics.underlineY);
        cooked.metrics.underline_thickness = static_cast<float>(metrics.underlineThickness);
        cooked.metrics.atlas_width = static_cast<std::uint32_t>(atlas_width);
        cooked.metrics.atlas_height = static_cast<std::uint32_t>(atlas_height);
        cooked.metrics.atlas_format = rhi::texture_format_t::rgba8_unorm;
        cooked.metrics.atlas_channel_layout = 1u;
        cooked.metrics.msdf_pixel_range = record.msdf.pixel_range;
        cooked.metrics.distance_normalization = record.msdf.pixel_range > 0.0f
            ? 1.0f / (record.msdf.pixel_range * 2.0f)
            : 1.0f;

        cooked.atlas_payload.resize(static_cast<std::size_t>(atlas_width) *
                                    static_cast<std::size_t>(atlas_height) * 4u);
        for (int y{ 0 }; y < atlas_height; ++y)
        {
            for (int x{ 0 }; x < atlas_width; ++x)
            {
                const msdf_atlas::byte* src{ atlas_bitmap(x, y) };
                const std::size_t dst_index{
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(atlas_width) +
                     static_cast<std::size_t>(x)) * 4u
                };
                cooked.atlas_payload[dst_index + 0u] = src[0];
                cooked.atlas_payload[dst_index + 1u] = src[1];
                cooked.atlas_payload[dst_index + 2u] = src[2];
                cooked.atlas_payload[dst_index + 3u] = src[3];
            }
        }

        cooked.glyphs.reserve(glyphs.size());
        std::unordered_map<std::uint32_t, std::uint32_t> glyph_index_to_codepoint;
        glyph_index_to_codepoint.reserve(glyphs.size());
        for (const msdf_atlas::GlyphGeometry& glyph : glyphs)
        {
            const std::uint32_t codepoint{ static_cast<std::uint32_t>(glyph.getCodepoint()) };
            if (codepoint == 0u)
                continue;

            glyph_index_to_codepoint.emplace(static_cast<std::uint32_t>(glyph.getGlyphIndex().getIndex()),
                                             codepoint);

            double plane_left{ 0.0 };
            double plane_bottom{ 0.0 };
            double plane_right{ 0.0 };
            double plane_top{ 0.0 };
            glyph.getQuadPlaneBounds(plane_left, plane_bottom, plane_right, plane_top);

            double atlas_left{ 0.0 };
            double atlas_bottom{ 0.0 };
            double atlas_right{ 0.0 };
            double atlas_top{ 0.0 };
            glyph.getQuadAtlasBounds(atlas_left, atlas_bottom, atlas_right, atlas_top);

            cooked.glyphs.push_back({
                .codepoint = codepoint,
                .glyph_index = static_cast<std::uint32_t>(glyph.getGlyphIndex().getIndex()),
                .advance = static_cast<float>(glyph.getAdvance()),
                .plane_left = static_cast<float>(plane_left),
                .plane_top = static_cast<float>(plane_top),
                .plane_right = static_cast<float>(plane_right),
                .plane_bottom = static_cast<float>(plane_bottom),
                .uv_left = static_cast<float>(atlas_left / static_cast<double>(atlas_width)),
                .uv_top = static_cast<float>((static_cast<double>(atlas_height) - atlas_top) / static_cast<double>(atlas_height)),
                .uv_right = static_cast<float>(atlas_right / static_cast<double>(atlas_width)),
                .uv_bottom = static_cast<float>((static_cast<double>(atlas_height) - atlas_bottom) / static_cast<double>(atlas_height)),
            });
        }

        std::sort(cooked.glyphs.begin(),
                  cooked.glyphs.end(),
                  [](const cfont_glyph_record_t& lhs, const cfont_glyph_record_t& rhs) noexcept
                  {
                      return lhs.codepoint < rhs.codepoint;
                  });

        if (record.msdf.include_kerning)
        {
            for (const auto& [pair, adjustment] : font_geometry.getKerning())
            {
                const auto left_it{
                    glyph_index_to_codepoint.find(static_cast<std::uint32_t>(pair.first))
                };
                const auto right_it{
                    glyph_index_to_codepoint.find(static_cast<std::uint32_t>(pair.second))
                };
                if (left_it == glyph_index_to_codepoint.end() || right_it == glyph_index_to_codepoint.end())
                    continue;

                cooked.kerning_pairs.push_back({
                    .left_codepoint = left_it->second,
                    .right_codepoint = right_it->second,
                    .adjustment = static_cast<float>(adjustment),
                });
            }

            std::sort(cooked.kerning_pairs.begin(),
                      cooked.kerning_pairs.end(),
                      [](const cfont_kerning_pair_t& lhs, const cfont_kerning_pair_t& rhs) noexcept
                      {
                          if (lhs.left_codepoint != rhs.left_codepoint)
                              return lhs.left_codepoint < rhs.left_codepoint;
                          return lhs.right_codepoint < rhs.right_codepoint;
                      });
        }

        return cooked;
    }
}
