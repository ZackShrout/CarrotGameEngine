//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "LoadedSpriteAsset.h"
#include "SpriteAssetRegistry.h"

#include <string_view>
#include <unordered_map>

namespace carrot::assets {
    class texture_asset_system_t;

    class sprite_asset_system_t
    {
    public:
        explicit sprite_asset_system_t(texture_asset_system_t& textures) noexcept
            : _textures{ textures } {}

        [[nodiscard]] const sprite_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] sprite_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] const loaded_sprite_asset_t* get(asset_id_t id);
        [[nodiscard]] const loaded_sprite_asset_t* get(std::string_view logical_id);

        void clear_runtime_cache();
        void clear_all();

    private:
        texture_asset_system_t& _textures;
        sprite_asset_registry_t _registry;
        std::unordered_map<asset_id_t, std::unique_ptr<loaded_sprite_asset_t>> _loaded;
    };
} // namespace carrot::assets
