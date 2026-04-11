//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FontAssetSystem.h"

#include "Assets/AssetID.h"

namespace carrot::assets {
    const loaded_font_asset_t* font_asset_system_t::get(const asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return it->second.get();

        const font_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Font asset id '{}' is not registered", id);
            return nullptr;
        }

        font_asset_load_result_t result{ load_font_asset(*record, _vfs, _rhi) };
        if (!result.success())
        {
            LOG_ASSET_ERROR("Failed to load font asset '{}' from '{}': {}",
                            record->logical_id,
                            record->source_uri,
                            to_string(result.error));
            return nullptr;
        }

        auto loaded_asset{ std::make_unique<loaded_font_asset_t>(std::move(result.asset)) };
        const loaded_font_asset_t* loaded_asset_ptr{ loaded_asset.get() };
        _loaded.emplace(id, std::move(loaded_asset));
        return loaded_asset_ptr;
    }

    const loaded_font_asset_t* font_asset_system_t::get(const std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void font_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void font_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
    }
} // namespace carrot::assets
