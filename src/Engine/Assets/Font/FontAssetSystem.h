//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
#include "FontAssetLoader.h"
#include "FontAssetRegistry.h"

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
    class font_asset_system_t
    {
    public:
        font_asset_system_t(io::virtual_file_system_t& vfs, rhi::rhi_context_t& rhi) noexcept
            : _vfs{ vfs }, _rhi{ rhi } {}

        [[nodiscard]] const font_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] font_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] const loaded_font_asset_t* get(asset_id_t id);
        [[nodiscard]] const loaded_font_asset_t* get(std::string_view logical_id);
        [[nodiscard]] std::vector<asset_iteration_status_t> collect_iteration_statuses() const;
        [[nodiscard]] std::optional<asset_iteration_status_t> find_iteration_status(asset_id_t id) const;

        void clear_runtime_cache();
        void clear_all();

    private:
        [[nodiscard]] asset_iteration_status_t make_status(const font_asset_record_t& record) const;
        void record_load_result(const font_asset_record_t& record,
                                const font_asset_load_result_t& result,
                                std::string_view error_message);

        io::virtual_file_system_t& _vfs;
        rhi::rhi_context_t& _rhi;
        font_asset_registry_t _registry;
        std::unordered_map<asset_id_t, std::unique_ptr<loaded_font_asset_t>> _loaded;
        std::unordered_map<asset_id_t, asset_iteration_status_t> _statuses;
    };
} // namespace carrot::assets
