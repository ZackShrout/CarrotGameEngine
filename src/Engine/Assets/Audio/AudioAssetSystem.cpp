//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioAssetSystem.h"

#include "Assets/AssetID.h"
#include "Assets/Audio/AudioAssetLoader.h"

namespace carrot::assets {
    const loaded_audio_asset_t* audio_asset_system_t::get(asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return &it->second;

        const audio_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Audio asset id '{}' is not registered", id);
            return nullptr;
        }

        audio_asset_load_result_t result{ load_audio_asset(*record, _vfs) };
        if (!result.success())
        {
            LOG_ASSET_ERROR("Failed to load audio asset '{}' from '{}': {}",
                            record->logical_id,
                            record->source_uri,
                            to_string(result.error));
            return nullptr;
        }

        const auto [it, inserted]{ _loaded.emplace(id, std::move(result.asset)) };

        return &it->second;
    }

    const loaded_audio_asset_t* audio_asset_system_t::get(std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void audio_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void audio_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
    }
}