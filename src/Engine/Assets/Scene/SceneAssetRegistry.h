//
// Created by zshrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetRegistry.h"
#include "Assets/Audio/AudioAssetRegistry.h"
#include "SceneAsset.h"
#include "Assets/Sprite/SpriteAssetRegistry.h"
#include "Assets/Tilemap/TilemapAssetRegistry.h"

#include <string_view>
#include <unordered_map>

namespace carrot::assets {
    class scene_asset_registry_t : public asset_registry_base_t
    {
    public:
        bool register_asset(scene_asset_record_t record);

        [[nodiscard]] const scene_asset_record_t* find(asset_id_t id) const noexcept;
        [[nodiscard]] const scene_asset_record_t* find(std::string_view logical_id) const noexcept;
        [[nodiscard]] const scene_asset_record_t* find_first_by_tilemap(std::string_view tilemap_id) const noexcept;
        [[nodiscard]] bool validate_references(const scene_asset_record_t& record,
                                               const tilemap_asset_registry_t& tilemaps,
                                               const sprite_asset_registry_t& sprites,
                                               const audio_asset_registry_t& audio) const noexcept;
        [[nodiscard]] bool contains(asset_id_t id) const noexcept { return _records.contains(id); }
        [[nodiscard]] const std::unordered_map<asset_id_t, scene_asset_record_t>& records() const noexcept { return _records; }

        void clear() noexcept { _records.clear(); }

    private:
        std::unordered_map<asset_id_t, scene_asset_record_t> _records;
    };
} // namespace carrot::assets
