//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioAsset.h"
#include "AudioAssetRegistry.h"

#include <string_view>
#include <unordered_map>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    class audio_asset_system_t
    {
    public:
        explicit audio_asset_system_t(io::virtual_file_system_t& vfs) noexcept : _vfs{ vfs } {}

        [[nodiscard]] const audio_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] audio_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] const loaded_audio_asset_t* get(asset_id_t id);
        [[nodiscard]] const loaded_audio_asset_t* get(std::string_view logical_id);

        void clear_runtime_cache();
        void clear_all();

    private:
        io::virtual_file_system_t& _vfs;
        audio_asset_registry_t _registry;
        std::unordered_map<asset_id_t, loaded_audio_asset_t> _loaded;
    };
} // namespace carrot::assets
