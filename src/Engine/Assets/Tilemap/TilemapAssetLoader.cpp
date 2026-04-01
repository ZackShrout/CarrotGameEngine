//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapAssetLoader.h"

namespace carrot::assets {
    tilemap_asset_load_result_t load_tilemap_asset(const tilemap_asset_record_t& record)
    {
        if (record.logical_id.empty())
        {
            return {
                .error = tilemap_asset_load_error_t::invalid_record
            };
        }

        return {
            .asset = loaded_tilemap_asset_t{ record.tilemap, &record },
            .error = tilemap_asset_load_error_t::none
        };
    }
} // namespace carrot::assets
