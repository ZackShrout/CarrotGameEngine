//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapWorldBridge.h"

#include "Assets/Tilemap/LoadedTilemapAsset.h"
#include "World/World.h"
#include "World/WorldUnits.h"

namespace carrot::world::import {
    tilemap_world_bridge_result_t import_tilemap_objects(world_t& world,
                                                         const assets::loaded_tilemap_asset_t& tilemap)
    {
        tilemap_world_bridge_result_t result{ };
        const assets::tilemap_asset_t& map{ tilemap.tilemap() };
        const assets::tilemap_asset_record_t* record{ tilemap.record() };

        for (const assets::tilemap_layer_t& layer : map.layers())
        {
            if (layer.kind != assets::tilemap_layer_kind_t::object)
                continue;

            for (const assets::tilemap_object_t& object : layer.objects)
            {
                world_object_t& world_object{ world.create_object() };
                world_object.name = object.name;
                world_object.type = object.type;
                world_object.source = world_object_source_t{
                    .tilemap_logical_id = record ? record->logical_id : std::string{},
                    .layer_name = layer.name,
                    .object_id = object.id,
                    .object_name = object.name
                };
                world_object.properties = object.properties;
                if (object.gid == 0)
                {
                    world_object.transform = transform_component_t{
                        .position = {
                            world_units_t::pixels_to_world(object.x),
                            world_units_t::pixels_to_world(object.y)
                        }
                    };
                    ++result.markers_created;
                    continue;
                }

                world_object.transform = transform_component_t{
                    .position = {
                        world_units_t::pixels_to_world(object.x),
                        world_units_t::pixels_to_world(object.y - object.height)
                    }
                };
                world_object.tile_object = tile_object_component_t{
                    .tilemap = &tilemap,
                    .gid = object.gid,
                    .size_source_px = { object.width, object.height },
                    .layer = renderer::render_layer_t::actors,
                    .order_mode = renderer::render_order_mode_t::anchor_bottom_y,
                    .order_in_layer = 0,
                    .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp,
                    .color = 0xFFFFFFFFu
                };

                ++result.tile_objects_created;
            }
        }

        return result;
    }
} // namespace carrot::world::import
