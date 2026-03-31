//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "LoadedSpriteAsset.h"
#include "SpriteAsset.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    struct sprite_asset_record_t;
    class texture_asset_system_t;

    enum class sprite_asset_load_error_t
    {
        none = 0,
        missing_texture_asset
    };

    struct sprite_asset_load_result_t
    {
        loaded_sprite_asset_t asset;
        sprite_asset_load_error_t error{ sprite_asset_load_error_t::none };

        [[nodiscard]] bool success() const noexcept
        {
            return error == sprite_asset_load_error_t::none;
        }
    };

    [[nodiscard]] sprite_asset_load_result_t load_sprite_asset(
        const sprite_asset_record_t& record,
        texture_asset_system_t& textures
    );
} // namespace carrot::assets
