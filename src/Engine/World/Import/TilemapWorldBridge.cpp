//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapWorldBridge.h"

#include "Assets/Tilemap/LoadedTilemapAsset.h"
#include "World/World.h"
#include "World/WorldLayering.h"
#include "World/WorldUnits.h"

namespace carrot::world::import {
    namespace {
        [[nodiscard]] std::optional<collision_debug_display_t> make_object_collision_debug_display(
            const assets::tilemap_object_t& object,
            const uint32_t default_color,
            const bool default_filled,
            const bool default_visible) noexcept
        {
            const bool show_collision{
                object.get_bool_property("debug_show_collision").value_or(default_visible)
            };
            if (!show_collision)
                return std::nullopt;

            return collision_debug_display_t{
                .filled = default_filled,
                .outline_thickness = 2.f,
                .color = default_color
            };
        }

        [[nodiscard]] size_t resolve_tileset_index(const std::vector<assets::tilemap_tileset_t>& tilesets, const uint32_t gid) noexcept
        {
            for (size_t i{ 0 }; i < tilesets.size(); ++i)
            {
                const uint32_t first_gid{ tilesets[i].first_gid };
                const uint32_t next_first_gid{
                    (i + 1u) < tilesets.size() ? tilesets[i + 1u].first_gid : std::numeric_limits<uint32_t>::max()
                };

                if (gid >= first_gid && gid < next_first_gid)
                    return i;
            }

            return static_cast<size_t>(-1);
        }

        void import_tileset_collision_for_tile_instance(world_t& world,
                                                        const assets::tilemap_tileset_t& tileset,
                                                        const uint32_t gid,
                                                        const chlm::float2 instance_origin_world,
                                                        const chlm::float2 scale,
                                                        tilemap_world_bridge_result_t& result)
        {
            const uint32_t local_tile_id{ gid - tileset.first_gid };
            const assets::tilemap_tileset_t::tile_collision_t* tile_collision{
                tileset.find_tile_collision(local_tile_id)
            };
            if (!tile_collision)
                return;

            for (const assets::tilemap_tileset_t::collision_rect_t& rect : tile_collision->collision_rects)
            {
                const chlm::float2 rect_origin{
                    instance_origin_world.x + (world_units_t::pixels_to_world(rect.x) * scale.x),
                    instance_origin_world.y + (world_units_t::pixels_to_world(rect.y) * scale.y)
                };
                const chlm::float2 rect_size{
                    world_units_t::pixels_to_world(rect.width) * scale.x,
                    world_units_t::pixels_to_world(rect.height) * scale.y
                };

                const auto& collider = world.collision_world().add_static_collider(collision::static_collider_t{
                    .bounds = collision::collision_aabb_t::from_min_size(rect_origin, rect_size)
                });
                static_cast<void>(collider);
                ++result.static_colliders_created;
            }
        }

        void import_tileset_collision(world_t& world,
                                      const assets::tilemap_asset_t& map,
                                      const chlm::float2 tilemap_origin_world,
                                      tilemap_world_bridge_result_t& result)
        {
            const auto& tilesets{ map.tilesets() };
            if (tilesets.empty())
                return;

            const float tile_width_world{ world_units_t::pixels_to_world(static_cast<float>(map.tile_width())) };
            const float tile_height_world{ world_units_t::pixels_to_world(static_cast<float>(map.tile_height())) };

            for (const assets::tilemap_layer_t& layer : map.layers())
            {
                if (layer.kind != assets::tilemap_layer_kind_t::tile || !layer.visible)
                    continue;

                for (uint32_t row{ 0 }; row < layer.height; ++row)
                {
                    for (uint32_t col{ 0 }; col < layer.width; ++col)
                    {
                        const size_t cell_index{ static_cast<size_t>(row) * static_cast<size_t>(layer.width) + static_cast<size_t>(col) };
                        if (cell_index >= layer.gids.size())
                            continue;

                        const uint32_t gid{ layer.gids[cell_index] };
                        if (gid == 0)
                            continue;

                        const size_t tileset_index{ resolve_tileset_index(tilesets, gid) };
                        if (tileset_index == static_cast<size_t>(-1))
                            continue;

                        const assets::tilemap_tileset_t& tileset{ tilesets[tileset_index] };

                        const chlm::float2 tile_origin{
                            tilemap_origin_world.x + (static_cast<float>(col) * tile_width_world),
                            tilemap_origin_world.y + (static_cast<float>(row) * tile_height_world)
                        };
                        import_tileset_collision_for_tile_instance(world,
                                                                   tileset,
                                                                   gid,
                                                                   tile_origin,
                                                                   chlm::float2{ 1.f, 1.f },
                                                                   result);
                    }
                }
            }
        }
    } // namespace

    tilemap_world_bridge_result_t import_tilemap_objects(world_t& world,
                                                         const assets::loaded_tilemap_asset_t& tilemap,
                                                         const chlm::float2 tilemap_origin_world)
    {
        tilemap_world_bridge_result_t result{ };
        const assets::tilemap_asset_t& map{ tilemap.tilemap() };
        const assets::tilemap_asset_record_t* record{ tilemap.record() };

        import_tileset_collision(world, map, tilemap_origin_world, result);

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
                            tilemap_origin_world.x + world_units_t::pixels_to_world(object.x),
                            tilemap_origin_world.y + world_units_t::pixels_to_world(object.y)
                        }
                    };

                    if (object.type == "Trigger" && object.width > 0.f && object.height > 0.f)
                    {
                        const chlm::float2 size_world{
                            world_units_t::pixels_to_world(object.width),
                            world_units_t::pixels_to_world(object.height)
                        };
                        world_object.collision = collision_component_t{
                            .half_extents = size_world * 0.5f,
                            .offset = size_world * 0.5f,
                            .debug_display = make_object_collision_debug_display(object, 0xFFFF00FFu, true, true)
                        };
                        world_object.trigger = trigger_component_t{
                            .trigger_id = std::string{ object.get_string_property("trigger_id").value_or(std::string_view{}) },
                            .trigger_kind = std::string{ object.get_string_property("trigger_kind").value_or(std::string_view{}) }
                        };
                        ++result.triggers_created;
                    }

                    if (const auto visibility_tag{ visibility_region_tag_for_object(object) };
                        visibility_tag && object.width > 0.f && object.height > 0.f)
                    {
                        world_object.visibility_region = visibility_region_component_t{
                            .size_world = {
                                world_units_t::pixels_to_world(object.width),
                                world_units_t::pixels_to_world(object.height)
                            },
                            .tag = std::string{ *visibility_tag }
                        };
                    }

                    ++result.markers_created;
                    continue;
                }

                world_object.transform = transform_component_t{
                    .position = {
                        tilemap_origin_world.x + world_units_t::pixels_to_world(object.x),
                        tilemap_origin_world.y + world_units_t::pixels_to_world(object.y - object.height)
                    }
                };
                const authored_layer_semantics_t layer_semantics{
                    resolve_object_layer_semantics(layer, 0)
                };
                world_object.tile_object = tile_object_component_t{
                    .tilemap = &tilemap,
                    .gid = object.gid,
                    .size_source_px = { object.width, object.height },
                    .layer = layer_semantics.render_layer,
                    .order_mode = layer_semantics.order_mode,
                    .order_in_layer = layer_semantics.order_in_layer,
                    .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp,
                    .color = 0xFFFFFFFFu
                };

                const auto& tilesets{ map.tilesets() };
                const size_t tileset_index{ resolve_tileset_index(tilesets, object.gid) };
                if (tileset_index != static_cast<size_t>(-1))
                {
                    const assets::tilemap_tileset_t& tileset{ tilesets[tileset_index] };
                    const chlm::float2 scale{
                        tileset.tile_width > 0
                            ? object.width / static_cast<float>(tileset.tile_width)
                            : 1.f,
                        tileset.tile_height > 0
                            ? object.height / static_cast<float>(tileset.tile_height)
                            : 1.f
                    };

                    import_tileset_collision_for_tile_instance(world,
                                                               tileset,
                                                               object.gid,
                                                               world_object.transform->position,
                                                               scale,
                                                               result);
                }

                ++result.tile_objects_created;
            }
        }

        return result;
    }
} // namespace carrot::world::import
