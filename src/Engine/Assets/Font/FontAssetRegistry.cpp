//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FontAssetRegistry.h"

namespace carrot::assets {
    bool font_asset_registry_t::register_asset(font_asset_record_t record)
    {
        if (record.id == 0 || record.logical_id.empty() || record.source_uri.empty())
            return false;

        return _records.emplace(record.id, std::move(record)).second;
    }

    const font_asset_record_t* font_asset_registry_t::find(const asset_id_t id) const noexcept
    {
        if (const auto it{ _records.find(id) }; it != _records.end())
            return &it->second;

        return nullptr;
    }

    const font_asset_record_t* font_asset_registry_t::find(const std::string_view logical_id) const noexcept
    {
        return find(make_asset_id(logical_id));
    }
} // namespace carrot::assets
