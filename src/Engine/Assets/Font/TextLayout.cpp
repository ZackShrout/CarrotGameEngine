//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextLayout.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] float compute_scale(const loaded_font_asset_t& font, const text_layout_settings_t& settings) noexcept
        {
            const float em_size{ font.cooked.metrics.em_size > 0.0f ? font.cooked.metrics.em_size : 1.0f };
            const float font_size{ settings.font_size > 0.0f ? settings.font_size : em_size };
            return font_size / em_size;
        }

        [[nodiscard]] float compute_line_height(const loaded_font_asset_t& font,
                                                const text_layout_settings_t& settings,
                                                const float scale) noexcept
        {
            const float base_line_height{
                font.cooked.metrics.line_height > 0.0f ? font.cooked.metrics.line_height : font.cooked.metrics.em_size
            };
            return (base_line_height * scale) + settings.line_spacing;
        }
    } // namespace

    text_layout_result_t layout_text(const loaded_font_asset_t& font,
                                     const std::string_view text,
                                     const text_layout_settings_t& settings) noexcept
    {
        text_layout_result_t result;
        if (!font.valid())
            return result;

        const float scale{ compute_scale(font, settings) };
        const float line_height{ compute_line_height(font, settings, scale) };
        float pen_x{ 0.0f };
        float pen_y{ 0.0f };
        float max_line_width{ 0.0f };
        std::size_t line_count{ 1u };
        std::uint32_t previous_codepoint{ 0u };
        bool has_previous_codepoint{ false };

        for (const char ch : text)
        {
            if (ch == '\r')
                continue;

            if (ch == '\n')
            {
                max_line_width = std::max(max_line_width, pen_x);
                pen_x = 0.0f;
                pen_y += line_height;
                previous_codepoint = 0u;
                has_previous_codepoint = false;
                ++line_count;
                continue;
            }

            const std::uint32_t codepoint{ static_cast<std::uint8_t>(ch) };
            const cfont_glyph_record_t* glyph{ font.find_glyph(codepoint) };
            if (!glyph)
                glyph = font.find_glyph(static_cast<std::uint32_t>('?'));

            if (!glyph)
                continue;

            if (has_previous_codepoint)
                pen_x += font.kerning_adjustment(previous_codepoint, glyph->codepoint) * scale;

            const float glyph_width{ (glyph->plane_right - glyph->plane_left) * scale };
            const float glyph_height{ (glyph->plane_top - glyph->plane_bottom) * scale };
            const float glyph_x{ pen_x + (glyph->plane_left * scale) };

            if (settings.wrap_width > 0.0f &&
                glyph_width > 0.0f &&
                pen_x > 0.0f &&
                (glyph_x + glyph_width) > settings.wrap_width)
            {
                max_line_width = std::max(max_line_width, pen_x);
                pen_x = 0.0f;
                pen_y += line_height;
                ++line_count;
                has_previous_codepoint = false;
                previous_codepoint = 0u;
            }

            result.glyphs.push_back({
                .glyph = glyph,
                .codepoint = glyph->codepoint,
                .x = pen_x + (glyph->plane_left * scale),
                .y = pen_y + ((font.cooked.metrics.ascent - glyph->plane_top) * scale),
                .width = glyph_width,
                .height = glyph_height,
                .u0 = glyph->uv_left,
                .v0 = glyph->uv_top,
                .u1 = glyph->uv_right,
                .v1 = glyph->uv_bottom,
            });

            pen_x += (glyph->advance * scale) + settings.letter_spacing;
            previous_codepoint = glyph->codepoint;
            has_previous_codepoint = true;
        }

        max_line_width = std::max(max_line_width, pen_x);
        result.bounds.width = max_line_width;
        result.bounds.height = line_count > 0u ? (static_cast<float>(line_count) * line_height) : 0.0f;
        result.line_count = text.empty() ? 0u : line_count;
        return result;
    }

    text_layout_bounds_t measure_text(const loaded_font_asset_t& font,
                                      const std::string_view text,
                                      const text_layout_settings_t& settings) noexcept
    {
        return layout_text(font, text, settings).bounds;
    }
} // namespace carrot::assets
