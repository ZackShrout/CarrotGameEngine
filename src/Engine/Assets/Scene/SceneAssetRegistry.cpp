//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SceneAssetRegistry.h"

namespace carrot::assets {
    bool scene_asset_registry_t::register_asset(scene_asset_record_t record)
    {
        if (record.id == 0 || _records.contains(record.id))
            return false;

        _records.emplace(record.id, std::move(record));
        return true;
    }

    const scene_asset_record_t* scene_asset_registry_t::find(const asset_id_t id) const noexcept
    {
        const auto it{ _records.find(id) };
        if (it == _records.end())
            return nullptr;

        return &it->second;
    }

    const scene_asset_record_t* scene_asset_registry_t::find(const std::string_view logical_id) const noexcept
    {
        return find(make_asset_id(logical_id));
    }

    const scene_asset_record_t* scene_asset_registry_t::find_first_by_tilemap(const std::string_view tilemap_id) const noexcept
    {
        for (const auto& [id, record] : _records)
        {
            if (record.scene.tilemap_id == tilemap_id)
                return &record;
        }

        return nullptr;
    }
} // namespace carrot::assets
