//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioAssetRegistry.h"

namespace carrot::assets {
    bool audio_asset_registry_t::register_asset(audio_asset_record_t record)
    {
        if (record.id == 0 || record.logical_id.empty())
            return false;

        return _records.emplace(record.id, std::move(record)).second;
    }

    const audio_asset_record_t* audio_asset_registry_t::find(const asset_id_t id) const noexcept
    {
        const auto it{ _records.find(id) };
        return it != _records.end() ? &it->second : nullptr;
    }

    const audio_asset_record_t* audio_asset_registry_t::find(const std::string_view logical_id) const noexcept
    {
        return find(make_asset_id(logical_id));
    }
} // namespace carrot::assets
