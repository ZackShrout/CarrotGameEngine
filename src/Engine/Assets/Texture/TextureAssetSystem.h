//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
#include "TextureAsset.h"
#include "TextureAssetRegistry.h"

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_context_t;
}

namespace carrot::assets {
    struct texture_asset_load_result_t;

    class texture_asset_system_t
    {
    public:
        texture_asset_system_t(io::virtual_file_system_t& vfs, rhi::rhi_context_t& rhi) noexcept
            : _vfs{ vfs }, _rhi{ rhi } {}

        [[nodiscard]] const texture_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] texture_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] const loaded_texture_asset_t* get(asset_id_t id);
        [[nodiscard]] const loaded_texture_asset_t* get(std::string_view logical_id);
        [[nodiscard]] std::vector<asset_iteration_status_t> collect_iteration_statuses() const;
        [[nodiscard]] std::optional<asset_iteration_status_t> find_iteration_status(asset_id_t id) const;
        bool reload(asset_id_t id);
        bool reload(std::string_view logical_id);

        void clear_runtime_cache();
        void clear_all();

    private:
        [[nodiscard]] asset_iteration_status_t make_status(const texture_asset_record_t& record) const;
        void record_load_result(const texture_asset_record_t& record,
                                const texture_asset_load_result_t& result,
                                std::string_view error_message);

        io::virtual_file_system_t& _vfs;
        rhi::rhi_context_t& _rhi;
        texture_asset_registry_t _registry;
        std::unordered_map<asset_id_t, std::unique_ptr<loaded_texture_asset_t>> _loaded;
        std::unordered_map<asset_id_t, asset_iteration_status_t> _statuses;
    };
} // namespace carrot::assets
