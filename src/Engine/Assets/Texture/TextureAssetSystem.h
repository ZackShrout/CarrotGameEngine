//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TextureAsset.h"
#include "TextureAssetRegistry.h"

#include <string_view>
#include <unordered_map>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_context_t;
}

namespace carrot::assets {
    class texture_asset_system_t
    {
    public:
        texture_asset_system_t(io::virtual_file_system_t& vfs, rhi::rhi_context_t& rhi) noexcept
            : _vfs{ vfs }, _rhi{ rhi } {}

        [[nodiscard]] const texture_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] texture_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] const loaded_texture_asset_t* get(asset_id_t id);
        [[nodiscard]] const loaded_texture_asset_t* get(std::string_view logical_id);

        void clear_runtime_cache();
        void clear_all();

    private:
        io::virtual_file_system_t& _vfs;
        rhi::rhi_context_t& _rhi;
        texture_asset_registry_t _registry;
        std::unordered_map<asset_id_t, loaded_texture_asset_t> _loaded;
    };
} // namespace carrot::assets