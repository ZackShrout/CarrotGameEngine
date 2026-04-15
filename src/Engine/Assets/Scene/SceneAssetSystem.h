//
// Created by zshrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
#include "SceneAssetRegistry.h"

#include <optional>
#include <vector>

namespace carrot::assets {
    class scene_asset_system_t
    {
    public:
        [[nodiscard]] const scene_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] scene_asset_registry_t& registry() noexcept { return _registry; }

        [[nodiscard]] std::vector<asset_iteration_status_t> collect_iteration_statuses() const;
        [[nodiscard]] std::optional<asset_iteration_status_t> find_iteration_status(asset_id_t id) const;
        void clear_all() noexcept { _registry.clear(); }

    private:
        scene_asset_registry_t _registry;
    };
} // namespace carrot::assets
