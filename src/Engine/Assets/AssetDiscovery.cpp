//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AssetDiscovery.h"

#include "IO/VirtualFileSystem.h"

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

            std::error_code status_error;
            const bool root_exists{ std::filesystem::exists(mount->root, status_error) };
            const bool root_is_directory{ root_exists && std::filesystem::is_directory(mount->root, status_error) };
            if (status_error || !root_exists || !root_is_directory)
            {
                LOG_ASSET_WARN("Asset discovery skipped mount '{}://': root '{}' is not a readable directory",
                               scheme,
                               mount->root.string());
                return;
            }

            std::error_code iterate_error;
            const std::filesystem::directory_options iterate_options{
                std::filesystem::directory_options::skip_permission_denied
            };
            for (std::filesystem::recursive_directory_iterator it{ mount->root, iterate_options, iterate_error }, end;
                 it != end;
                 it.increment(iterate_error))
            {
                if (iterate_error)
                {
                    LOG_ASSET_WARN("Asset discovery stopped early for mount '{}://{}': {}",
                                   scheme,
                                   mount->root.string(),
                                   iterate_error.message());
                    break;
                }

                const auto& entry{ *it };
                if (!entry.is_regular_file())
                    continue;

                const std::filesystem::path relative_path{ std::filesystem::relative(entry.path(), mount->root) };
                const std::string relative_generic{ relative_path.generic_string() };
                const std::string virtual_path{ std::string{ scheme } + "://" + relative_generic };

                if (ends_with(relative_generic, ".audio.json"))
                {
                    manifests.audio.emplace_back(virtual_path);
                }
                else if (ends_with(relative_generic, ".font.json"))
                {
                    manifests.fonts.emplace_back(virtual_path);
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
                else if (ends_with(relative_generic, ".scene.json"))
                {
                    manifests.scenes.emplace_back(virtual_path);
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
        sort_manifest_group(manifests.fonts);
        sort_manifest_group(manifests.textures);
        sort_manifest_group(manifests.sprites);
        sort_manifest_group(manifests.tilemaps);
        sort_manifest_group(manifests.scenes);

        return manifests;
    }
} // namespace carrot::assets
