//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetID.h"
#include "Assets/AssetRegistry.h"
#include "SpriteAsset.h"

#include <string_view>
#include <unordered_map>

namespace carrot::assets {
    class sprite_asset_registry_t : public asset_registry_base_t
    {
    public:
        sprite_asset_registry_t() = default;

        bool register_asset(sprite_asset_record_t record);

        [[nodiscard]] const sprite_asset_record_t* find(asset_id_t id) const noexcept;
        [[nodiscard]] const sprite_asset_record_t* find(std::string_view logical_id) const noexcept;
        [[nodiscard]] bool contains(const asset_id_t id) const noexcept { return _records.contains(id); }
        [[nodiscard]] const std::unordered_map<asset_id_t, sprite_asset_record_t>& records() const noexcept { return _records; }

        void clear() noexcept { _records.clear(); }

    private:
        std::unordered_map<asset_id_t, sprite_asset_record_t> _records;
    };
} // namespace carrot::assets
