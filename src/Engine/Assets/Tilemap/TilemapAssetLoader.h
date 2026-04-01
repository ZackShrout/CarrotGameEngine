//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "LoadedTilemapAsset.h"
#include "TilemapAsset.h"

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
        decode_failed,
        texture_create_failed
    };

    struct tilemap_asset_load_result_t
    {
        loaded_tilemap_asset_t asset;
        tilemap_asset_load_error_t error{ tilemap_asset_load_error_t::none };

        [[nodiscard]] bool success() const noexcept
        {
            return error == tilemap_asset_load_error_t::none;
        }
    };

    [[nodiscard]] tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record);
    [[nodiscard]] tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record,
                                                                 const io::virtual_file_system_t& vfs,
                                                                 rhi::rhi_context_t& rhi) noexcept;
} // namespace carrot::assets
