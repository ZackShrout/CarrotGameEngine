//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>
#include <cstdint>

namespace carrot::assets {
    class loaded_tilemap_asset_t;
}

namespace carrot::world {
    class world_t;
}

namespace carrot::world::import {
    struct tilemap_world_bridge_result_t
    {
        uint32_t markers_created{ 0 };
        uint32_t tile_objects_created{ 0 };
        uint32_t static_colliders_created{ 0 };
        uint32_t triggers_created{ 0 };
    };

    [[nodiscard]] tilemap_world_bridge_result_t import_tilemap_objects(
        world_t& world,
        const assets::loaded_tilemap_asset_t& tilemap,
        chlm::float2 tilemap_origin_world = chlm::float2{ 0.f, 0.f });
} // namespace carrot::world::import
