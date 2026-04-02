//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AssetDiscovery.h"

#include "IO/VirtualFileSystem.h"

#include <algorithm>
#include <array>
#include <filesystem>

namespace carrot::assets {
    namespace {
        [[nodiscard]] bool ends_with(const std::string_view value, const std::string_view suffix) noexcept
        {
            return value.size() >= suffix.size() &&
                   value.substr(value.size() - suffix.size()) == suffix;
        }

        void discover_manifests_in_scheme(discovered_asset_manifests_t& manifests,
                                          const io::virtual_file_system_t& vfs,
                                          const std::string_view scheme)
        {
            const auto mount{ vfs.get_mount(scheme) };
            if (!mount)
                return;

            for (const std::filesystem::recursive_directory_iterator it{ mount->root }, end; auto const& entry : it)
            {
                if (!entry.is_regular_file())
                    continue;

                const std::filesystem::path relative_path{ std::filesystem::relative(entry.path(), mount->root) };
                const std::string relative_generic{ relative_path.generic_string() };
                const std::string virtual_path{ std::string{ scheme } + "://" + relative_generic };

                if (ends_with(relative_generic, ".audio.json"))
                {
                    manifests.audio.emplace_back(virtual_path);
                }
                else if (ends_with(relative_generic, ".texture.json"))
                {
                    manifests.textures.emplace_back(virtual_path);
                }
                else if (ends_with(relative_generic, ".sprite.json"))
                {
                    manifests.sprites.emplace_back(virtual_path);
                }
                else if (ends_with(relative_generic, ".tilemap.json"))
                {
                    manifests.tilemaps.emplace_back(virtual_path);
                }
            }
        }

        void sort_manifest_group(std::vector<std::string>& manifests)
        {
            std::sort(manifests.begin(), manifests.end());
        }
    } // namespace

    discovered_asset_manifests_t asset_discovery_t::discover_supported_manifests(const io::virtual_file_system_t& vfs)
    {
        discovered_asset_manifests_t manifests{ };
        constexpr std::array<std::string_view, 2> schemes{ "engine", "game" };

        for (const std::string_view scheme : schemes)
            discover_manifests_in_scheme(manifests, vfs, scheme);

        sort_manifest_group(manifests.audio);
        sort_manifest_group(manifests.textures);
        sort_manifest_group(manifests.sprites);
        sort_manifest_group(manifests.tilemaps);

        return manifests;
    }
} // namespace carrot::assets
