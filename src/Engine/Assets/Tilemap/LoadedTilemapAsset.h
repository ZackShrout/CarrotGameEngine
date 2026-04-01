//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TilemapAsset.h"

namespace carrot::assets {
    class loaded_tilemap_asset_t
    {
    public:
        loaded_tilemap_asset_t() = default;
        loaded_tilemap_asset_t(tilemap_asset_t tilemap, const tilemap_asset_record_t* record) noexcept
            : _tilemap{ std::move(tilemap) }, _record{ record } {}

        [[nodiscard]] const tilemap_asset_t& tilemap() const noexcept { return _tilemap; }
        [[nodiscard]] const tilemap_asset_record_t* record() const noexcept { return _record; }
        [[nodiscard]] bool valid() const noexcept { return _record != nullptr; }

    private:
        tilemap_asset_t _tilemap;
        const tilemap_asset_record_t* _record{ nullptr };
    };
} // namespace carrot::assets
