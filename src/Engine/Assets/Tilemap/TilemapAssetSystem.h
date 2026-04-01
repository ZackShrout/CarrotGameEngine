//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "LoadedTilemapAsset.h"
#include "TilemapAssetRegistry.h"

#include "RHI/RHI.h"

#include <string_view>
#include <unordered_map>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    class tilemap_asset_system_t
    {
    public:
        tilemap_asset_system_t(io::virtual_file_system_t& vfs, rhi::rhi_context_t& rhi) noexcept
            : _vfs{ vfs }, _rhi{ rhi } {}

        [[nodiscard]] const tilemap_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] tilemap_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] const loaded_tilemap_asset_t* get(asset_id_t id);
        [[nodiscard]] const loaded_tilemap_asset_t* get(std::string_view logical_id);

        void clear_runtime_cache();
        void clear_all();

    private:
        io::virtual_file_system_t& _vfs;
        rhi::rhi_context_t& _rhi;
        tilemap_asset_registry_t _registry;
        std::unordered_map<asset_id_t, loaded_tilemap_asset_t> _loaded;
    };
} // namespace carrot::assets
