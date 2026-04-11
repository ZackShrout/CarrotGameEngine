//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetID.h"
#include "CookedFont.h"
#include "RHI/Texture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace carrot::assets {
    enum class font_charset_preset_t : std::uint8_t
    {
        basic_latin = 0,
    };

    struct font_msdf_import_settings_t
    {
        std::uint32_t atlas_width{ 1024u };
        std::uint32_t atlas_height{ 1024u };
        float pixel_range{ 8.0f };
        bool include_kerning{ true };
    };

    struct font_defaults_t
    {
        float line_height_scale{ 1.0f };
    };

    struct font_asset_record_t
    {
        asset_id_t id{ 0 };
        std::string logical_id;
        std::string source_uri;
        std::string manifest_uri;
        std::uint32_t schema_version{ 1u };
        font_charset_preset_t charset_preset{ font_charset_preset_t::basic_latin };
        std::vector<std::uint32_t> include_codepoints;
        std::vector<std::uint32_t> exclude_codepoints;
        font_msdf_import_settings_t msdf;
        font_defaults_t defaults;
    };

    struct loaded_font_asset_t
    {
        const font_asset_record_t* record{ nullptr };
        cooked_font_data_t cooked;
        std::unique_ptr<rhi::rhi_texture_t> atlas_texture{ nullptr };

        [[nodiscard]] bool valid() const noexcept
        {
            return record != nullptr && atlas_texture != nullptr;
        }

        [[nodiscard]] const cfont_glyph_record_t* find_glyph(std::uint32_t codepoint) const noexcept
        {
            const auto it{
                std::lower_bound(cooked.glyphs.begin(),
                                 cooked.glyphs.end(),
                                 codepoint,
                                 [](const cfont_glyph_record_t& glyph, const std::uint32_t value) noexcept
                                 {
                                     return glyph.codepoint < value;
                                 })
            };

            if (it == cooked.glyphs.end() || it->codepoint != codepoint)
                return nullptr;

            return &(*it);
        }

        [[nodiscard]] float kerning_adjustment(std::uint32_t left_codepoint,
                                               std::uint32_t right_codepoint) const noexcept
        {
            const auto it{
                std::lower_bound(cooked.kerning_pairs.begin(),
                                 cooked.kerning_pairs.end(),
                                 std::pair{ left_codepoint, right_codepoint },
                                 [](const cfont_kerning_pair_t& pair,
                                    const std::pair<std::uint32_t, std::uint32_t>& value) noexcept
                                 {
                                     if (pair.left_codepoint != value.first)
                                         return pair.left_codepoint < value.first;

                                     return pair.right_codepoint < value.second;
                                 })
            };

            if (it == cooked.kerning_pairs.end() ||
                it->left_codepoint != left_codepoint ||
                it->right_codepoint != right_codepoint)
            {
                return 0.0f;
            }

            return it->adjustment;
        }
    };
} // namespace carrot::assets
