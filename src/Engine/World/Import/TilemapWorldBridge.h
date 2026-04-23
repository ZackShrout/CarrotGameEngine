//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Collision/CollisionWorld.h"
#include "World/Components/CollisionComponent.h"
#include "World/Components/AuthoredGeometryComponent.h"
#include "World/Components/TileObjectComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Components/TriggerComponent.h"
#include "World/Components/VisibilityRegionComponent.h"
#include "World/WorldObject.h"

#include <chlm/CarrotHLM.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace carrot::assets {
    class loaded_tilemap_asset_t;
}

namespace carrot::world {
    class world_t;
}

namespace carrot::world::import {
    struct prepared_tile_object_component_t
    {
        uint32_t gid{ 0 };
        chlm::float2 size_source_px{ 0.f, 0.f };
        renderer::render_layer_t layer{ renderer::render_layer_t::actors };
        renderer::render_order_mode_t order_mode{ renderer::render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        renderer::quad_sampler_preset_t sampler_preset{ renderer::quad_sampler_preset_t::pixel_clamp };
        uint32_t color{ 0xFFFFFFFFu };
    };

    struct prepared_world_object_t
    {
        std::string name;
        std::string type;
        std::optional<world_object_source_t> source;
        std::vector<assets::tilemap_property_t> properties;
        std::optional<transform_component_t> transform;
        std::optional<collision_component_t> collision;
        std::optional<authored_geometry_component_t> authored_geometry;
        std::optional<trigger_component_t> trigger;
        std::optional<prepared_tile_object_component_t> tile_object;
        std::optional<visibility_region_component_t> visibility_region;
    };

    struct prepared_tilemap_world_data_t
    {
        std::vector<prepared_world_object_t> objects;
        std::vector<collision::static_collider_t> static_colliders;
    };

    struct tilemap_world_bridge_result_t
    {
        uint32_t markers_created{ 0 };
        uint32_t tile_objects_created{ 0 };
        uint32_t static_colliders_created{ 0 };
        uint32_t triggers_created{ 0 };
    };

    [[nodiscard]] prepared_tilemap_world_data_t prepare_tilemap_world_data(
        const assets::loaded_tilemap_asset_t& tilemap,
        chlm::float2 tilemap_origin_world = chlm::float2{ 0.f, 0.f });
    [[nodiscard]] tilemap_world_bridge_result_t apply_prepared_tilemap_world_data(
        world_t& world,
        const assets::loaded_tilemap_asset_t& tilemap,
        const prepared_tilemap_world_data_t& prepared);
    [[nodiscard]] tilemap_world_bridge_result_t import_tilemap_objects(
        world_t& world,
        const assets::loaded_tilemap_asset_t& tilemap,
        chlm::float2 tilemap_origin_world = chlm::float2{ 0.f, 0.f });
} // namespace carrot::world::import
