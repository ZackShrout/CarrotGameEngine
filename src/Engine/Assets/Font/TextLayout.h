//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "FontAsset.h"

#include <string_view>
#include <vector>

namespace carrot::assets {
    struct text_layout_settings_t
    {
        float font_size{ 16.0f };
        float wrap_width{ 0.0f };
        float letter_spacing{ 0.0f };
        float line_spacing{ 0.0f };
    };

    struct text_layout_bounds_t
    {
        float width{ 0.0f };
        float height{ 0.0f };
    };

    struct positioned_glyph_t
    {
        const cfont_glyph_record_t* glyph{ nullptr };
        std::uint32_t codepoint{ 0u };
        float x{ 0.0f };
        float y{ 0.0f };
        float width{ 0.0f };
        float height{ 0.0f };
        float u0{ 0.0f };
        float v0{ 0.0f };
        float u1{ 0.0f };
        float v1{ 0.0f };
    };

    struct text_layout_result_t
    {
        std::vector<positioned_glyph_t> glyphs;
        text_layout_bounds_t bounds;
        std::size_t line_count{ 0u };
    };

    [[nodiscard]] text_layout_result_t layout_text(const loaded_font_asset_t& font,
                                                   std::string_view text,
                                                   const text_layout_settings_t& settings = {}) noexcept;
    [[nodiscard]] text_layout_bounds_t measure_text(const loaded_font_asset_t& font,
                                                    std::string_view text,
                                                    const text_layout_settings_t& settings = {}) noexcept;
} // namespace carrot::assets
