//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetRegistry.h"
#include "FontAsset.h"

#include <string_view>
#include <unordered_map>

namespace carrot::assets {
    class font_asset_registry_t final : public asset_registry_base_t
    {
    public:
        bool register_asset(font_asset_record_t record);

        [[nodiscard]] const font_asset_record_t* find(asset_id_t id) const noexcept;
        [[nodiscard]] const font_asset_record_t* find(std::string_view logical_id) const noexcept;
        [[nodiscard]] bool contains(asset_id_t id) const noexcept { return _records.contains(id); }
        [[nodiscard]] const std::unordered_map<asset_id_t, font_asset_record_t>& records() const noexcept { return _records; }

        void clear() noexcept { _records.clear(); }

    private:
        std::unordered_map<asset_id_t, font_asset_record_t> _records;
    };
} // namespace carrot::assets
