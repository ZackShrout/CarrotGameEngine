//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapAssetSystem.h"

#include "Assets/AssetID.h"
#include "TilemapAssetLoader.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] std::string_view to_string(const tilemap_asset_load_error_t error) noexcept
        {
            switch (error)
            {
                case tilemap_asset_load_error_t::none: return "none";
                case tilemap_asset_load_error_t::invalid_record: return "invalid_record";
                case tilemap_asset_load_error_t::resolve_failed: return "resolve_failed";
                case tilemap_asset_load_error_t::source_not_found: return "source_not_found";
                case tilemap_asset_load_error_t::manifest_not_found: return "manifest_not_found";
                case tilemap_asset_load_error_t::decode_failed: return "decode_failed";
                case tilemap_asset_load_error_t::texture_create_failed: return "texture_create_failed";
                case tilemap_asset_load_error_t::cooked_write_failed: return "cooked_write_failed";
                default: return "unknown";
            }
        }
    }

    const loaded_tilemap_asset_t* tilemap_asset_system_t::get(const asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return &it->second;

        const tilemap_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Tilemap asset id '{}' is not registered", id);
            return nullptr;
        }

        tilemap_asset_load_result_t result{ load_tilemap_asset(*record, _vfs, _rhi) };
        if (!result.success())
        {
            LOG_ASSET_ERROR("Failed to load tilemap asset '{}': {}", record->logical_id, to_string(result.error));
            return nullptr;
        }

        const auto [it, inserted]{ _loaded.emplace(id, std::move(result.asset)) };
        return &it->second;
    }

    const loaded_tilemap_asset_t* tilemap_asset_system_t::get(const std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void tilemap_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void tilemap_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
    }
} // namespace carrot::assets
