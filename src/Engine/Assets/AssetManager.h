//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/AudioAssetSystem.h"
#include "Texture/TextureAssetSystem.h"

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
        explicit asset_manager_t(io::virtual_file_system_t& vfs, rhi::rhi_context_t& rhi) noexcept : _vfs{ vfs }, _audio{ vfs }, _textures { vfs, rhi } {}

        [[nodiscard]] const io::virtual_file_system_t& vfs() const noexcept { return _vfs; }
        [[nodiscard]] io::virtual_file_system_t& vfs() noexcept { return _vfs; }

        [[nodiscard]] const audio_asset_system_t& audio() const noexcept { return _audio; }
        [[nodiscard]] audio_asset_system_t& audio() noexcept { return _audio; }

        [[nodiscard]] const texture_asset_system_t& textures() const noexcept { return _textures; }
        [[nodiscard]] texture_asset_system_t& textures() noexcept { return _textures; }

        void clear();

    private:
        io::virtual_file_system_t& _vfs;
        audio_asset_system_t _audio;
        texture_asset_system_t _textures;
    };
} // namespace carrot::assets
