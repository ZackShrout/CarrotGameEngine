//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
#include "LoadedSpriteAsset.h"
#include "SpriteAssetRegistry.h"

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    class texture_asset_system_t;
    struct sprite_asset_load_result_t;

    class sprite_asset_system_t
    {
    public:
        sprite_asset_system_t(io::virtual_file_system_t& vfs, texture_asset_system_t& textures) noexcept
            : _vfs{ vfs }, _textures{ textures } {}

        [[nodiscard]] const sprite_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] sprite_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] const loaded_sprite_asset_t* get(asset_id_t id);
        [[nodiscard]] const loaded_sprite_asset_t* get(std::string_view logical_id);
        [[nodiscard]] std::vector<asset_iteration_status_t> collect_iteration_statuses() const;
        [[nodiscard]] std::optional<asset_iteration_status_t> find_iteration_status(asset_id_t id) const;
        bool reload(asset_id_t id);
        bool reload(std::string_view logical_id);

        void clear_runtime_cache();
        void clear_all();

    private:
        [[nodiscard]] asset_iteration_status_t make_status(const sprite_asset_record_t& record) const;
        void record_load_result(const sprite_asset_record_t& record,
                                const sprite_asset_load_result_t& result,
                                std::string_view error_message);

        io::virtual_file_system_t& _vfs;
        texture_asset_system_t& _textures;
        sprite_asset_registry_t _registry;
        std::unordered_map<asset_id_t, std::unique_ptr<loaded_sprite_asset_t>> _loaded;
        std::unordered_map<asset_id_t, asset_iteration_status_t> _statuses;
    };
} // namespace carrot::assets
