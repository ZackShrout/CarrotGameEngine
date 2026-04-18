//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AssetIteration.h"
#include "Audio/AudioAssetSystem.h"
#include "Font/FontAssetSystem.h"
#include "Scene/SceneAssetSystem.h"
#include "Sprite/SpriteAssetSystem.h"
#include "Texture/TextureAssetSystem.h"
#include "Tilemap/TilemapAssetSystem.h"

#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

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
        explicit asset_manager_t(io::virtual_file_system_t& vfs, rhi::rhi_context_t& rhi) noexcept
            : _vfs{ vfs }, _audio{ vfs }, _fonts{ vfs, rhi }, _textures{ vfs, rhi }, _sprites{ vfs, _textures }, _tilemaps{ vfs, rhi }, _scenes{} {}

        [[nodiscard]] const io::virtual_file_system_t& vfs() const noexcept { return _vfs; }
        [[nodiscard]] io::virtual_file_system_t& vfs() noexcept { return _vfs; }

        [[nodiscard]] const audio_asset_system_t& audio() const noexcept { return _audio; }
        [[nodiscard]] audio_asset_system_t& audio() noexcept { return _audio; }

        [[nodiscard]] const font_asset_system_t& fonts() const noexcept { return _fonts; }
        [[nodiscard]] font_asset_system_t& fonts() noexcept { return _fonts; }

        [[nodiscard]] const texture_asset_system_t& textures() const noexcept { return _textures; }
        [[nodiscard]] texture_asset_system_t& textures() noexcept { return _textures; }

        [[nodiscard]] const sprite_asset_system_t& sprites() const noexcept { return _sprites; }
        [[nodiscard]] sprite_asset_system_t& sprites() noexcept { return _sprites; }

        [[nodiscard]] const tilemap_asset_system_t& tilemaps() const noexcept { return _tilemaps; }
        [[nodiscard]] tilemap_asset_system_t& tilemaps() noexcept { return _tilemaps; }

        [[nodiscard]] const scene_asset_system_t& scenes() const noexcept { return _scenes; }
        [[nodiscard]] scene_asset_system_t& scenes() noexcept { return _scenes; }

        [[nodiscard]] std::vector<asset_iteration_status_t> collect_runtime_iteration_statuses() const;
        [[nodiscard]] std::optional<asset_iteration_status_t> find_runtime_iteration_status(asset_kind_t kind,
                                                                                            asset_id_t id) const;
        bool reload_asset(asset_kind_t kind, asset_id_t id);
        bool reload_asset(asset_kind_t kind, std::string_view logical_id);
        void poll_runtime_iteration_changes();

        void clear();

    private:
        struct runtime_iteration_watch_snapshot_t
        {
            std::optional<std::filesystem::file_time_type> source_write_time;
            std::optional<std::filesystem::file_time_type> manifest_write_time;
        };

        struct runtime_iteration_diagnostics_t
        {
            asset_iteration_watch_change_t last_watch_change{ asset_iteration_watch_change_t::none };
            asset_iteration_request_origin_t last_refresh_request_origin{ asset_iteration_request_origin_t::none };
            asset_runtime_refresh_action_t last_requested_action{ asset_runtime_refresh_action_t::none };
        };

        [[nodiscard]] static std::uint64_t runtime_iteration_watch_key(asset_kind_t kind, asset_id_t id) noexcept;
        [[nodiscard]] runtime_iteration_watch_snapshot_t capture_watch_snapshot(std::string_view source_uri,
                                                                                std::string_view manifest_uri) const;
        [[nodiscard]] asset_iteration_status_t enrich_runtime_iteration_status(asset_iteration_status_t status) const;
        void note_runtime_refresh_request(asset_kind_t kind,
                                          asset_id_t id,
                                          asset_iteration_request_origin_t origin,
                                          asset_runtime_refresh_action_t action);
        bool reload_asset(asset_kind_t kind, asset_id_t id, asset_iteration_request_origin_t origin);
        void update_runtime_iteration_watch(asset_kind_t kind,
                                            asset_id_t id,
                                            std::string_view logical_id,
                                            std::string_view source_uri,
                                            std::string_view manifest_uri);

        io::virtual_file_system_t& _vfs;
        audio_asset_system_t _audio;
        font_asset_system_t _fonts;
        texture_asset_system_t _textures;
        sprite_asset_system_t _sprites;
        tilemap_asset_system_t _tilemaps;
        scene_asset_system_t _scenes;
        std::unordered_map<std::uint64_t, runtime_iteration_watch_snapshot_t> _runtime_iteration_watches;
        std::unordered_map<std::uint64_t, runtime_iteration_diagnostics_t> _runtime_iteration_diagnostics;
    };
} // namespace carrot::assets
