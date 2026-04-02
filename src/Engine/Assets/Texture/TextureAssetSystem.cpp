//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextureAssetSystem.h"

#include "Assets/AssetID.h"
#include "TextureAssetLoader.h"

namespace carrot::assets {
    const loaded_texture_asset_t* texture_asset_system_t::get(asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return &it->second;

        const texture_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Texture asset id '{}' is not registered", id);
            return nullptr;
        }

        texture_asset_load_result_t result{ load_texture_asset(*record, _vfs, _rhi) };
        if (!result.success())
        {
            LOG_ASSET_ERROR(
                "Failed to load texture asset '{}' from '{}': {}",
                record->logical_id,
                record->source_uri,
                to_string(result.error)
            );
            return nullptr;
        }

        const auto [it, inserted]{ _loaded.emplace(id, std::move(result.asset)) };
        return &it->second;
    }

    const loaded_texture_asset_t* texture_asset_system_t::get(std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void texture_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void texture_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
    }
} // namespace carrot::assets