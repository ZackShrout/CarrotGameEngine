//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "SceneAssetRegistry.h"

namespace carrot::assets {
    class scene_asset_system_t
    {
    public:
        [[nodiscard]] const scene_asset_registry_t& registry() const noexcept { return _registry; }
        [[nodiscard]] scene_asset_registry_t& registry() noexcept { return _registry; }

        void clear_all() noexcept { _registry.clear(); }

    private:
        scene_asset_registry_t _registry;
    };
} // namespace carrot::assets
