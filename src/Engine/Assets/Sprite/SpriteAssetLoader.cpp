//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SpriteAssetLoader.h"
#include "Assets/Texture/TextureAssetSystem.h"

namespace carrot::assets {
    sprite_asset_load_result_t load_sprite_asset( const sprite_asset_record_t& record, texture_asset_system_t& textures)
    {
        const loaded_texture_asset_t* texture{ textures.get(record.sprite.texture_id()) };
        if (!texture)
        {
            return {
                .asset = { },
                .error = sprite_asset_load_error_t::missing_texture_asset
            };
        }

        return {
            .asset = loaded_sprite_asset_t{ record.sprite, texture },
            .error = sprite_asset_load_error_t::none
        };
    }
} // namespace carrot::assets
