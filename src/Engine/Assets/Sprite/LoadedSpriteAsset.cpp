//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "LoadedSpriteAsset.h"

namespace carrot::assets {
    loaded_sprite_asset_t::loaded_sprite_asset_t(sprite_asset_t sprite, const loaded_texture_asset_t* texture)
        : _sprite{ std::move(sprite) }, _texture{ texture } {}

    const sprite_frame_t* loaded_sprite_asset_t::find_frame(std::string_view name) const noexcept
    {
        return _sprite.find_frame(name);
    }

    const sprite_animation_t* loaded_sprite_asset_t::find_animation(std::string_view name) const noexcept
    {
        return _sprite.find_animation(name);
    }
} // namespace carrot::assets
