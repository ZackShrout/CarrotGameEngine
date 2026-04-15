//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
#include "Assets/ImportedAssetCache.h"
#include "LoadedTilemapAsset.h"
#include "TilemapAsset.h"
#include "RHI/RHI.h"

#include <filesystem>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_context_t;
}

namespace carrot::assets {
    enum class tilemap_asset_load_error_t
    {
        none = 0,
        invalid_record,
        resolve_failed,
        source_not_found,
        manifest_not_found,
        decode_failed,
        texture_create_failed,
        cooked_write_failed
    };

    struct tilemap_asset_load_result_t
    {
        loaded_tilemap_asset_t asset;
        tilemap_asset_load_error_t error{ tilemap_asset_load_error_t::none };
        asset_load_origin_t load_origin{ asset_load_origin_t::never_loaded };
        imported_artifact_state_t cooked_artifact_state{ imported_artifact_state_t::missing };
        imported_artifact_issue_t invalidation_reason{ imported_artifact_issue_t::none };

        [[nodiscard]] bool success() const noexcept
        {
            return error == tilemap_asset_load_error_t::none;
        }
    };

    struct prepared_tilemap_tileset_texture_t
    {
        uint32_t width{ 0u };
        uint32_t height{ 0u };
        size_t stride_bytes{ 0u };
        rhi::texture_format_t format{ rhi::texture_format_t::rgba8_unorm };
        std::vector<std::uint8_t> pixel_payload;
    };

    struct prepared_tilemap_asset_t
    {
        tilemap_asset_t tilemap;
        const tilemap_asset_record_t* record{ nullptr };
        std::vector<prepared_tilemap_tileset_texture_t> tileset_textures;
        imported_asset_invalidation_t expected_invalidation;
        std::filesystem::path cooked_path;
        bool write_cooked_artifact{ false };
    };

    struct tilemap_asset_prepare_result_t
    {
        prepared_tilemap_asset_t asset;
        tilemap_asset_load_error_t error{ tilemap_asset_load_error_t::none };

        [[nodiscard]] bool success() const noexcept
        {
            return error == tilemap_asset_load_error_t::none;
        }
    };

    [[nodiscard]] tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record);
    [[nodiscard]] tilemap_asset_prepare_result_t prepare_tilemap_asset(const tilemap_asset_record_t& record,
                                                                       const io::virtual_file_system_t& vfs) noexcept;
    [[nodiscard]] tilemap_asset_load_result_t realize_prepared_tilemap_asset(prepared_tilemap_asset_t prepared,
                                                                              rhi::rhi_context_t& rhi) noexcept;
    [[nodiscard]] tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record,
                                                                 const io::virtual_file_system_t& vfs,
                                                                 rhi::rhi_context_t& rhi) noexcept;
    [[nodiscard]] std::filesystem::path cooked_tilemap_cache_path(std::string_view logical_id,
                                                                  const io::virtual_file_system_t& vfs) noexcept;
} // namespace carrot::assets
