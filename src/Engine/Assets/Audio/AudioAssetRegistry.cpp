//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioAssetRegistry.h"

namespace carrot::assets {
    audio_asset_handle audio_asset_registry_t::register_asset(const asset_id_t id, const audio_asset_t& asset)
    {
        if (_id_to_index.contains(id))
        {
            // duplicate asset ID = fatal authoring error
            return { };
        }

        const uint32_t index{ static_cast<uint32_t>(_assets.size()) };

        _assets.push_back(entry_t{
            .asset = asset,
            .generation = 1
        });

        _id_to_index[id] = index;

        return audio_asset_handle{
            .index = index,
            .generation = 1
        };
    }

    const audio_asset_t* audio_asset_registry_t::get(const audio_asset_handle handle) const noexcept
    {
        if (!is_valid(handle))
            return nullptr;

        return &_assets[handle.index].asset;
    }

    audio_asset_handle audio_asset_registry_t::find(const asset_id_t id) const noexcept
    {
        if (!_id_to_index.contains(id))
            return { };

        const uint32_t index{ _id_to_index.at(id) };

        return audio_asset_handle{
            .index = index,
            .generation = _assets[index].generation
        };
    }

    bool audio_asset_registry_t::contains(const asset_id_t id) const noexcept
    {
        return _id_to_index.contains(id);
    }

    bool audio_asset_registry_t::is_valid(audio_asset_handle handle) const noexcept
    {
        if (handle.index >= _assets.size())
            return false;

        return _assets[handle.index].generation == handle.generation;
    }

    void audio_asset_registry_t::clear()
    {
        _assets.clear();
        _id_to_index.clear();
    }
} // namespace carrot::assets
