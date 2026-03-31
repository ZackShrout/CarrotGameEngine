//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SpriteAssetRegistry.h"

namespace carrot::assets {
    bool sprite_asset_registry_t::register_asset(sprite_asset_record_t record)
    {
        if (record.id == 0 || record.logical_id.empty())
            return false;

        if (!is_valid_logical_asset_id(record.logical_id))
            return false;

        if (record.sprite.texture_id().empty())
            return false;

        return _records.emplace(record.id, std::move(record)).second;
    }

    const sprite_asset_record_t* sprite_asset_registry_t::find(const asset_id_t id) const noexcept
    {
        const auto it{ _records.find(id) };
        return it != _records.end() ? &it->second : nullptr;
    }

    const sprite_asset_record_t* sprite_asset_registry_t::find(const std::string_view logical_id) const noexcept
    {
        return find(make_asset_id(logical_id));
    }
} // namespace carrot::assets
