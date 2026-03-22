//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TextureAsset.h"
#include "Assets/AssetID.h"
#include "Assets/AssetRegistry.h"

#include <string_view>
#include <unordered_map>

namespace carrot::assets {
    class texture_asset_registry_t : public asset_registry_base_t
    {
    public:
        texture_asset_registry_t() = default;

        bool register_asset(texture_asset_record_t record);

        [[nodiscard]] const texture_asset_record_t* find(asset_id_t id) const noexcept;
        [[nodiscard]] const texture_asset_record_t* find(std::string_view logical_id) const noexcept;
        [[nodiscard]] bool contains(const asset_id_t id) const noexcept { return _records.contains(id); }

        void clear() noexcept { _records.clear(); }

    private:
        std::unordered_map<asset_id_t, texture_asset_record_t> _records;
    };
} // namespace carrot::assets
