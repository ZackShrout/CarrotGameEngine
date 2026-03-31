//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SpriteAssetSystem.h"

#include "Assets/AssetID.h"
#include "SpriteAssetLoader.h"

namespace carrot::assets {
    const loaded_sprite_asset_t* sprite_asset_system_t::get(const asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return &it->second;

        const sprite_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Sprite asset id '{}' is not registered", id);
            return nullptr;
        }

        sprite_asset_load_result_t result{ load_sprite_asset(*record, _textures) };
        if (!result.success())
        {
            LOG_ASSET_ERROR(
                "Failed to load sprite asset '{}': missing texture asset '{}'",
                record->logical_id,
                record->sprite.texture_id()
            );
            return nullptr;
        }

        const auto [it, inserted]{ _loaded.emplace(id, std::move(result.asset)) };
        return &it->second;
    }

    const loaded_sprite_asset_t* sprite_asset_system_t::get(const std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void sprite_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void sprite_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
    }
} // namespace carrot::assets
