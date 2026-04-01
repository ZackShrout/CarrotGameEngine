//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapAssetRegistry.h"

namespace carrot::assets {
    bool tilemap_asset_registry_t::register_asset(tilemap_asset_record_t record)
    {
        if (_records.contains(record.id))
            return false;

        _records.emplace(record.id, std::move(record));
        return true;
    }

    const tilemap_asset_record_t* tilemap_asset_registry_t::find(const asset_id_t id) const noexcept
    {
        if (const auto it{ _records.find(id) }; it != _records.end())
            return &it->second;

        return nullptr;
    }

    const tilemap_asset_record_t* tilemap_asset_registry_t::find(const std::string_view logical_id) const noexcept
    {
        return find(make_asset_id(logical_id));
    }
} // namespace carrot::assets
