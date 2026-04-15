//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
#include "LoadedTilemapAsset.h"
#include "RHI/RHI.h"
#include "TilemapAssetRegistry.h"

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    struct tilemap_asset_load_result_t;

    class tilemap_asset_system_t
    {
    public:
        tilemap_asset_system_t(io::virtual_file_system_t& vfs, rhi::rhi_context_t& rhi) noexcept
            : _vfs{ vfs }, _rhi{ rhi } {}

        [[nodiscard]] const tilemap_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] tilemap_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] const loaded_tilemap_asset_t* get(asset_id_t id);
        [[nodiscard]] const loaded_tilemap_asset_t* get(std::string_view logical_id);
        [[nodiscard]] std::vector<asset_iteration_status_t> collect_iteration_statuses() const;
        [[nodiscard]] std::optional<asset_iteration_status_t> find_iteration_status(asset_id_t id) const;
        [[nodiscard]] bool is_loaded(asset_id_t id) const noexcept { return _loaded.contains(id); }
        [[nodiscard]] bool is_loaded(std::string_view logical_id) const noexcept
        {
            const tilemap_asset_record_t* record{ _registry.find(logical_id) };
            return record != nullptr && is_loaded(record->id);
        }
        void cache_loaded(asset_id_t id, loaded_tilemap_asset_t asset);

        void clear_runtime_cache();
        void clear_all();

    private:
        [[nodiscard]] asset_iteration_status_t make_status(const tilemap_asset_record_t& record) const;
        void record_load_result(const tilemap_asset_record_t& record,
                                const tilemap_asset_load_result_t& result,
                                std::string_view error_message);

        io::virtual_file_system_t& _vfs;
        rhi::rhi_context_t& _rhi;
        tilemap_asset_registry_t _registry;
        std::unordered_map<asset_id_t, loaded_tilemap_asset_t> _loaded;
        std::unordered_map<asset_id_t, asset_iteration_status_t> _statuses;
    };
} // namespace carrot::assets
