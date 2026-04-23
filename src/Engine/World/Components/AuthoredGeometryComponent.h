#pragma once

#include "Assets/Tilemap/TilemapAsset.h"

#include <chlm/CarrotHLM.h>

#include <vector>

namespace carrot::world {
    struct authored_geometry_component_t
    {
        assets::tilemap_object_t::geometry_kind_t kind{ assets::tilemap_object_t::geometry_kind_t::rectangle };
        chlm::float2 size_source_px{ 0.f, 0.f };
        std::vector<chlm::float2> points_source_px;
    };
} // namespace carrot::world
