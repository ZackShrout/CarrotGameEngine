//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    struct discovered_asset_manifests_t
    {
        std::vector<std::string> audio;
        std::vector<std::string> textures;
        std::vector<std::string> sprites;
        std::vector<std::string> tilemaps;

        [[nodiscard]] size_t total_count() const noexcept
        {
            return audio.size() + textures.size() + sprites.size() + tilemaps.size();
        }
    };

    class asset_discovery_t
    {
    public:
        [[nodiscard]] static discovered_asset_manifests_t discover_supported_manifests(
            const io::virtual_file_system_t& vfs);
    };
} // namespace carrot::assets
