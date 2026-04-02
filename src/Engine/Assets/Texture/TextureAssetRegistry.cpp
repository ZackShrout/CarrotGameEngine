//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextureAssetRegistry.h"

namespace carrot::assets {
    bool texture_asset_registry_t::register_asset(texture_asset_record_t record)
    {
        if (record.id == 0 || record.logical_id.empty() || record.source_uri.empty())
            return false;

        if (!is_valid_logical_asset_id(record.logical_id))
            return false;

        return _records.emplace(record.id, std::move(record)).second;
    }

    const texture_asset_record_t* texture_asset_registry_t::find(const asset_id_t id) const noexcept
    {
        const auto it{ _records.find(id) };
        return it != _records.end() ? &it->second : nullptr;
    }

    const texture_asset_record_t* texture_asset_registry_t::find(const std::string_view logical_id) const noexcept
    {
        return find(make_asset_id(logical_id));
    }
} // namespace carrot::assets
