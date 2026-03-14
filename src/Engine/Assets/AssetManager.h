//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/AudioAssetRegistry.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    /**
     * @brief Central owner of runtime asset registries and asset loading services.
     *
     * The asset manager owns the concrete asset registries used by the engine at
     * runtime and provides a single point of access for loading, lookup, and
     * lifetime management of assets.
     *
     * Initially this class exposes registry access and clearing behavior. Over time
     * it will also absorb typed asset loading, loader registration, path-based
     * lookup, deduplication, and hot reload behavior.
     */
    class asset_manager_t
    {
    public:
        explicit asset_manager_t(io::virtual_file_system_t& vfs) noexcept : _vfs{ vfs } {}

        [[nodiscard]] const io::virtual_file_system_t& vfs() const noexcept { return _vfs; }
        [[nodiscard]] io::virtual_file_system_t& vfs() noexcept { return _vfs; }

        [[nodiscard]] const audio_asset_registry_t& audio() const noexcept { return _audio; }
        [[nodiscard]] audio_asset_registry_t& audio() noexcept { return _audio; }

        void clear();

    private:
        io::virtual_file_system_t& _vfs;
        audio_asset_registry_t _audio;
    };
} // namespace carrot::assets
