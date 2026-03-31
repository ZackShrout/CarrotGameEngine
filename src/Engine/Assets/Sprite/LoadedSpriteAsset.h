//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Sprite/SpriteAsset.h"
#include "Assets/Texture/TextureAsset.h"

namespace carrot::assets {
    class loaded_sprite_asset_t
    {
    public:
        loaded_sprite_asset_t() = default;
        loaded_sprite_asset_t(sprite_asset_t sprite, const loaded_texture_asset_t* texture);

        [[nodiscard]] const sprite_asset_t& sprite() const noexcept { return _sprite; }
        [[nodiscard]] const loaded_texture_asset_t* texture() const noexcept { return _texture; }

        [[nodiscard]] const sprite_frame_t* find_frame(std::string_view name) const noexcept;
        [[nodiscard]] const sprite_animation_t* find_animation(std::string_view name) const noexcept;

        [[nodiscard]] bool valid() const noexcept { return _texture != nullptr; }

    private:
        sprite_asset_t _sprite;
        const loaded_texture_asset_t* _texture{ nullptr };
    };
} // namespace carrot::assets
