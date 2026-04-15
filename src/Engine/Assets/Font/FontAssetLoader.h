//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
#include "Assets/ImportedAssetCache.h"
#include "FontAsset.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_context_t;
}

namespace carrot::assets {
    enum class font_asset_load_error_t
    {
        ok,
        invalid_record,
        source_not_found,
        manifest_not_found,
        save_mount_unavailable,
        cooked_invalid_format,
        cooked_invalid_atlas_dimensions,
        cooked_invalid_distance_settings,
        cooked_invalid_tables,
        cooked_invalid_atlas_format,
        cooked_invalid_atlas_payload,
        generator_failed,
        cooked_serialize_failed,
        cooked_write_failed,
        cooked_read_failed,
        texture_create_failed,
    };

    struct font_asset_load_result_t
    {
        loaded_font_asset_t asset;
        font_asset_load_error_t error{ font_asset_load_error_t::ok };
        asset_load_origin_t load_origin{ asset_load_origin_t::never_loaded };
        imported_artifact_state_t cooked_artifact_state{ imported_artifact_state_t::missing };
        imported_artifact_issue_t invalidation_reason{ imported_artifact_issue_t::none };

        [[nodiscard]] bool success() const noexcept
        {
            return error == font_asset_load_error_t::ok;
        }
    };

    [[nodiscard]] font_asset_load_result_t load_font_asset(const font_asset_record_t& record,
                                                           const io::virtual_file_system_t& vfs,
                                                           rhi::rhi_context_t& rhi) noexcept;

    [[nodiscard]] std::filesystem::path cooked_font_cache_path(std::string_view logical_id,
                                                               const io::virtual_file_system_t& vfs) noexcept;

    [[nodiscard]] constexpr std::string_view to_string(const font_asset_load_error_t error) noexcept
    {
        switch (error)
        {
            case font_asset_load_error_t::ok: return "ok";
            case font_asset_load_error_t::invalid_record: return "invalid_record";
            case font_asset_load_error_t::source_not_found: return "source_not_found";
            case font_asset_load_error_t::manifest_not_found: return "manifest_not_found";
            case font_asset_load_error_t::save_mount_unavailable: return "save_mount_unavailable";
            case font_asset_load_error_t::cooked_invalid_format: return "cooked_invalid_format";
            case font_asset_load_error_t::cooked_invalid_atlas_dimensions: return "cooked_invalid_atlas_dimensions";
            case font_asset_load_error_t::cooked_invalid_distance_settings: return "cooked_invalid_distance_settings";
            case font_asset_load_error_t::cooked_invalid_tables: return "cooked_invalid_tables";
            case font_asset_load_error_t::cooked_invalid_atlas_format: return "cooked_invalid_atlas_format";
            case font_asset_load_error_t::cooked_invalid_atlas_payload: return "cooked_invalid_atlas_payload";
            case font_asset_load_error_t::generator_failed: return "generator_failed";
            case font_asset_load_error_t::cooked_serialize_failed: return "cooked_serialize_failed";
            case font_asset_load_error_t::cooked_write_failed: return "cooked_write_failed";
            case font_asset_load_error_t::cooked_read_failed: return "cooked_read_failed";
            case font_asset_load_error_t::texture_create_failed: return "texture_create_failed";
            default: return "unknown_error";
        }
    }
} // namespace carrot::assets
