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
        constexpr float k_collision_bake_weld_tolerance_world{ world_units_t::pixels_to_world(1.0f) };
        constexpr float k_collision_bake_collinear_tolerance_world{ world_units_t::pixels_to_world(0.5f) };
        constexpr float k_collision_bake_area_tolerance_world_sq{ world_units_t::pixels_to_world(2.0f) * world_units_t::pixels_to_world(2.0f) };

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

        [[nodiscard]] float distance_between_points(const chlm::float2 a, const chlm::float2 b) noexcept
        {
            const chlm::float2 delta{ a - b };
            return std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
        }

        [[nodiscard]] bool nearly_same_point(const chlm::float2 a,
                                             const chlm::float2 b,
                                             const float tolerance = k_collision_bake_weld_tolerance_world) noexcept
        {
            return distance_between_points(a, b) <= tolerance;
        }

        [[nodiscard]] std::vector<chlm::float2> rectangle_points_world(const collision::collision_aabb_t& bounds)
        {
            return {
                chlm::float2{ bounds.min.x, bounds.min.y },
                chlm::float2{ bounds.max.x, bounds.min.y },
                chlm::float2{ bounds.max.x, bounds.max.y },
                chlm::float2{ bounds.min.x, bounds.max.y }
            };
        }

        [[nodiscard]] float signed_polygon_area(const std::vector<chlm::float2>& points) noexcept
        {
            if (points.size() < 3u)
                return 0.f;

            float twice_area{ 0.f };
            for (size_t i{ 0u }; i < points.size(); ++i)
            {
                const chlm::float2 a{ points[i] };
                const chlm::float2 b{ points[(i + 1u) % points.size()] };
                twice_area += (a.x * b.y) - (a.y * b.x);
            }

            return twice_area * 0.5f;
        }

        [[nodiscard]] float polygon_area(const std::vector<chlm::float2>& points) noexcept
        {
            return std::fabs(signed_polygon_area(points));
        }

        [[nodiscard]] chlm::float2 cross_product_2d(const chlm::float2 a, const chlm::float2 b) noexcept
        {
            return chlm::float2{ 0.f, (a.x * b.y) - (a.y * b.x) };
        }

        [[nodiscard]] bool nearly_collinear(const chlm::float2 a,
                                            const chlm::float2 b,
                                            const chlm::float2 c,
                                            const float tolerance = k_collision_bake_collinear_tolerance_world) noexcept
        {
            const chlm::float2 ab{ b - a };
            const chlm::float2 bc{ c - b };
            return std::fabs(cross_product_2d(ab, bc).y) <= tolerance;
        }

        void remove_duplicate_consecutive_points(std::vector<chlm::float2>& points) noexcept
        {
            if (points.empty())
                return;

            std::vector<chlm::float2> compacted;
            compacted.reserve(points.size());
            compacted.push_back(points.front());
            for (size_t i{ 1u }; i < points.size(); ++i)
            {
                if (!nearly_same_point(points[i], compacted.back()))
                    compacted.push_back(points[i]);
            }

            if (compacted.size() >= 2u && nearly_same_point(compacted.front(), compacted.back()))
                compacted.pop_back();

            points = std::move(compacted);
        }

        void simplify_polygon_points(std::vector<chlm::float2>& points) noexcept
        {
            remove_duplicate_consecutive_points(points);
            if (points.size() < 3u)
                return;

            bool changed{ true };
            while (changed && points.size() >= 3u)
            {
                changed = false;
                for (size_t i{ 0u }; i < points.size(); ++i)
                {
                    const chlm::float2 prev{ points[(i + points.size() - 1u) % points.size()] };
                    const chlm::float2 curr{ points[i] };
                    const chlm::float2 next{ points[(i + 1u) % points.size()] };
                    if (nearly_same_point(prev, curr) ||
                        nearly_same_point(curr, next) ||
                        nearly_collinear(prev, curr, next))
                    {
                        points.erase(points.begin() + static_cast<std::ptrdiff_t>(i));
                        changed = true;
                        break;
                    }
                }
            }
        }

        [[nodiscard]] std::vector<chlm::float2> convex_hull(std::vector<chlm::float2> points)
        {
            remove_duplicate_consecutive_points(points);
            if (points.size() < 3u)
                return points;

            std::sort(points.begin(), points.end(), [](const chlm::float2& lhs, const chlm::float2& rhs) {
                if (lhs.x != rhs.x)
                    return lhs.x < rhs.x;
                return lhs.y < rhs.y;
            });

            std::vector<chlm::float2> lower;
            for (const chlm::float2 point : points)
            {
                while (lower.size() >= 2u)
                {
                    const chlm::float2 a{ lower[lower.size() - 2u] };
                    const chlm::float2 b{ lower[lower.size() - 1u] };
                    const float cross{ ((b.x - a.x) * (point.y - a.y)) - ((b.y - a.y) * (point.x - a.x)) };
                    if (cross > 1.0e-6f)
                        break;
                    lower.pop_back();
                }
                lower.push_back(point);
            }

            std::vector<chlm::float2> upper;
            for (auto it{ points.rbegin() }; it != points.rend(); ++it)
            {
                while (upper.size() >= 2u)
                {
                    const chlm::float2 a{ upper[upper.size() - 2u] };
                    const chlm::float2 b{ upper[upper.size() - 1u] };
                    const float cross{ ((b.x - a.x) * (it->y - a.y)) - ((b.y - a.y) * (it->x - a.x)) };
                    if (cross > 1.0e-6f)
                        break;
                    upper.pop_back();
                }
                upper.push_back(*it);
            }

            lower.pop_back();
            upper.pop_back();
            lower.insert(lower.end(), upper.begin(), upper.end());
            return lower;
        }

        [[nodiscard]] bool is_axis_aligned_rectangle(const std::vector<chlm::float2>& points) noexcept
        {
            if (points.size() != 4u)
                return false;

            const collision::collision_aabb_t bounds{
                .min = {
                    std::min({ points[0].x, points[1].x, points[2].x, points[3].x }),
                    std::min({ points[0].y, points[1].y, points[2].y, points[3].y })
                },
                .max = {
                    std::max({ points[0].x, points[1].x, points[2].x, points[3].x }),
                    std::max({ points[0].y, points[1].y, points[2].y, points[3].y })
                }
            };

            const std::vector<chlm::float2> rect{ rectangle_points_world(bounds) };
            for (const chlm::float2 point : points)
            {
                bool matched{ false };
                for (const chlm::float2 corner : rect)
                {
                    if (nearly_same_point(point, corner))
                    {
                        matched = true;
                        break;
                    }
                }
                if (!matched)
                    return false;
            }

            return true;
        }

        [[nodiscard]] collision::static_collider_t collider_from_polygon(std::vector<chlm::float2> points,
                                                                         const collision::collision_layer_t layer,
                                                                         const collision::collision_mask_t mask)
        {
            simplify_polygon_points(points);
            if (is_axis_aligned_rectangle(points))
            {
                collision::collision_aabb_t bounds{
                    .min = {
                        std::min({ points[0].x, points[1].x, points[2].x, points[3].x }),
                        std::min({ points[0].y, points[1].y, points[2].y, points[3].y })
                    },
                    .max = {
                        std::max({ points[0].x, points[1].x, points[2].x, points[3].x }),
                        std::max({ points[0].y, points[1].y, points[2].y, points[3].y })
                    }
                };
                return collision::static_collider_t{
                    .bounds = bounds,
                    .layer = layer,
                    .mask = mask
                };
            }

            return collision::static_collider_t{
                .shape = collision::static_collider_t::shape_t::convex_polygon,
                .polygon_points = std::move(points),
                .layer = layer,
                .mask = mask
            };
        }

        [[nodiscard]] std::vector<chlm::float2> collider_points(const collision::static_collider_t& collider)
        {
            return collider.is_convex_polygon() ? collider.polygon_points : rectangle_points_world(collider.bounds);
        }

        [[nodiscard]] chlm::float2 weld_point(const chlm::float2 point,
                                              std::vector<chlm::float2>& representatives,
                                              const float tolerance = k_collision_bake_weld_tolerance_world)
        {
            for (const chlm::float2 representative : representatives)
            {
                if (nearly_same_point(point, representative, tolerance))
                    return representative;
            }

            representatives.push_back(point);
            return point;
        }

        void normalize_static_collider_points(std::vector<collision::static_collider_t>& static_colliders)
        {
            std::vector<chlm::float2> representatives;
            for (collision::static_collider_t& collider : static_colliders)
            {
                std::vector<chlm::float2> points{ collider_points(collider) };
                for (chlm::float2& point : points)
                    point = weld_point(point, representatives);

                collider = collider_from_polygon(std::move(points), collider.layer, collider.mask);
            }
        }

        [[nodiscard]] bool try_merge_colliders(const collision::static_collider_t& lhs,
                                               const collision::static_collider_t& rhs,
                                               collision::static_collider_t& merged_out)
        {
            if (lhs.layer != rhs.layer || lhs.mask != rhs.mask)
                return false;

            std::vector<chlm::float2> merged_points{ collider_points(lhs) };
            for (const chlm::float2 point : collider_points(rhs))
            {
                bool exists{ false };
                for (const chlm::float2 existing : merged_points)
                {
                    if (nearly_same_point(point, existing))
                    {
                        exists = true;
                        break;
                    }
                }
                if (!exists)
                    merged_points.push_back(point);
            }

            const std::vector<chlm::float2> hull{ convex_hull(merged_points) };
            if (hull.size() < 3u)
                return false;

            const float merged_area{ polygon_area(hull) };
            const float original_area{ polygon_area(collider_points(lhs)) + polygon_area(collider_points(rhs)) };
            if (std::fabs(merged_area - original_area) > k_collision_bake_area_tolerance_world_sq)
                return false;

            merged_out = collider_from_polygon(hull, lhs.layer, lhs.mask);
            return true;
        }

        void bake_static_colliders(std::vector<collision::static_collider_t>& static_colliders)
        {
            if (static_colliders.size() < 2u)
                return;

            normalize_static_collider_points(static_colliders);

            bool merged_any{ true };
            while (merged_any)
            {
                merged_any = false;
                for (size_t i{ 0u }; i < static_colliders.size() && !merged_any; ++i)
                {
                    for (size_t j{ i + 1u }; j < static_colliders.size(); ++j)
                    {
                        collision::static_collider_t merged;
                        if (!try_merge_colliders(static_colliders[i], static_colliders[j], merged))
                            continue;

                        static_colliders[i] = std::move(merged);
                        static_colliders.erase(static_colliders.begin() + static_cast<std::ptrdiff_t>(j));
                        merged_any = true;
                        break;
                    }
                }
            }
        }

        void collect_tileset_collision(std::vector<collision::static_collider_t>& static_colliders,
                                       const assets::tilemap_tileset_t& tileset,
                                       const uint32_t gid,
                                       const chlm::float2 instance_origin_world,
                                       const chlm::float2 scale)
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

                static_colliders.push_back(collision::static_collider_t{
                    .bounds = collision::collision_aabb_t::from_min_size(rect_origin, rect_size)
                });
            }

            for (const assets::tilemap_tileset_t::collision_polygon_t& polygon : tile_collision->collision_polygons)
            {
                std::vector<chlm::float2> points_world;
                points_world.reserve(polygon.points.size());
                for (const chlm::float2 point_px : polygon.points)
                {
                    points_world.push_back(chlm::float2{
                        instance_origin_world.x + (world_units_t::pixels_to_world(point_px.x) * scale.x),
                        instance_origin_world.y + (world_units_t::pixels_to_world(point_px.y) * scale.y)
                    });
                }

                if (points_world.size() >= 3u)
                {
                    static_colliders.push_back(collision::static_collider_t{
                        .shape = collision::static_collider_t::shape_t::convex_polygon,
                        .polygon_points = std::move(points_world)
                    });
                }
            }
        }

        void collect_tileset_collision(std::vector<collision::static_collider_t>& static_colliders,
                                       const assets::tilemap_asset_t& map,
                                       const chlm::float2 tilemap_origin_world)
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
                        collect_tileset_collision(static_colliders,
                                                  tileset,
                                                  gid,
                                                  tile_origin,
                                                  chlm::float2{ 1.f, 1.f });
                    }
                }
            }
        }

        prepared_world_object_t make_prepared_marker_object(const assets::tilemap_object_t& object,
                                                            const assets::tilemap_layer_t& layer,
                                                            const std::string& logical_id,
                                                            const chlm::float2 tilemap_origin_world)
        {
            prepared_world_object_t prepared{
                .name = object.name,
                .type = object.type,
                .source = world_object_source_t{
                    .tilemap_logical_id = logical_id,
                    .layer_name = layer.name,
                    .object_id = object.id,
                    .object_name = object.name
                },
                .properties = object.properties,
                .transform = transform_component_t{
                    .position = {
                        tilemap_origin_world.x + world_units_t::pixels_to_world(object.x),
                        tilemap_origin_world.y + world_units_t::pixels_to_world(object.y)
                    }
                },
                .authored_geometry = authored_geometry_component_t{
                    .kind = object.geometry_kind,
                    .size_source_px = { object.width, object.height },
                    .points_source_px = object.geometry_points
                }
            };

            if (object.type == "Trigger" && object.width > 0.f && object.height > 0.f)
            {
                const chlm::float2 size_world{
                    world_units_t::pixels_to_world(object.width),
                    world_units_t::pixels_to_world(object.height)
                };
                prepared.collision = collision_component_t{
                    .half_extents = size_world * 0.5f,
                    .offset = size_world * 0.5f,
                    .debug_display = make_object_collision_debug_display(object, 0xFFFF00FFu, true, true)
                };
                prepared.trigger = trigger_component_t{
                    .trigger_id = std::string{ object.get_string_property("trigger_id").value_or(std::string_view{}) },
                    .trigger_kind = std::string{ object.get_string_property("trigger_kind").value_or(std::string_view{}) }
                };
            }

            if (const auto visibility_tag{ visibility_region_tag_for_object(object) };
                visibility_tag && object.width > 0.f && object.height > 0.f)
            {
                prepared.visibility_region = visibility_region_component_t{
                    .size_world = {
                        world_units_t::pixels_to_world(object.width),
                        world_units_t::pixels_to_world(object.height)
                    },
                    .tag = std::string{ *visibility_tag }
                };
            }

            return prepared;
        }

        prepared_world_object_t make_prepared_tile_object(const assets::loaded_tilemap_asset_t& tilemap,
                                                          const assets::tilemap_layer_t& layer,
                                                          const assets::tilemap_object_t& object,
                                                          const std::string& logical_id,
                                                          const chlm::float2 tilemap_origin_world,
                                                          std::vector<collision::static_collider_t>& static_colliders)
        {
            prepared_world_object_t prepared{
                .name = object.name,
                .type = object.type,
                .source = world_object_source_t{
                    .tilemap_logical_id = logical_id,
                    .layer_name = layer.name,
                    .object_id = object.id,
                    .object_name = object.name
                },
                .properties = object.properties,
                .transform = transform_component_t{
                    .position = {
                        tilemap_origin_world.x + world_units_t::pixels_to_world(object.x),
                        tilemap_origin_world.y + world_units_t::pixels_to_world(object.y - object.height)
                    }
                }
            };

            const authored_layer_semantics_t layer_semantics{
                resolve_object_layer_semantics(layer, 0)
            };
            prepared.tile_object = prepared_tile_object_component_t{
                .gid = object.gid,
                .size_source_px = { object.width, object.height },
                .layer = layer_semantics.render_layer,
                .order_mode = layer_semantics.order_mode,
                .order_in_layer = layer_semantics.order_in_layer,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp,
                .color = 0xFFFFFFFFu
            };

            const auto& tilesets{ tilemap.tilemap().tilesets() };
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

                collect_tileset_collision(static_colliders,
                                          tileset,
                                          object.gid,
                                          prepared.transform->position,
                                          scale);
            }

            return prepared;
        }
    } // namespace

    prepared_tilemap_world_data_t prepare_tilemap_world_data(const assets::loaded_tilemap_asset_t& tilemap,
                                                             const chlm::float2 tilemap_origin_world)
    {
        const assets::tilemap_asset_t& map{ tilemap.tilemap() };
        const assets::tilemap_asset_record_t* record{ tilemap.record() };
        const std::string logical_id{ record ? record->logical_id : std::string{} };

        prepared_tilemap_world_data_t prepared;
        collect_tileset_collision(prepared.static_colliders, map, tilemap_origin_world);

        for (const assets::tilemap_layer_t& layer : map.layers())
        {
            if (layer.kind != assets::tilemap_layer_kind_t::object)
                continue;

            for (const assets::tilemap_object_t& object : layer.objects)
            {
                if (object.gid == 0)
                {
                    prepared.objects.push_back(make_prepared_marker_object(object,
                                                                           layer,
                                                                           logical_id,
                                                                           tilemap_origin_world));
                    continue;
                }

                prepared.objects.push_back(make_prepared_tile_object(tilemap,
                                                                     layer,
                                                                     object,
                                                                     logical_id,
                                                                     tilemap_origin_world,
                                                                     prepared.static_colliders));
            }
        }

        bake_static_colliders(prepared.static_colliders);

        return prepared;
    }

    tilemap_world_bridge_result_t apply_prepared_tilemap_world_data(world_t& world,
                                                                    const assets::loaded_tilemap_asset_t& tilemap,
                                                                    const prepared_tilemap_world_data_t& prepared)
    {
        tilemap_world_bridge_result_t result{ };

        for (const collision::static_collider_t& collider : prepared.static_colliders)
        {
            (void)world.collision_world().add_static_collider(collider);
            ++result.static_colliders_created;
        }

        for (const prepared_world_object_t& prepared_object : prepared.objects)
        {
            world_object_t& world_object{ world.create_object() };
            world_object.name = prepared_object.name;
            world_object.type = prepared_object.type;
            world_object.source = prepared_object.source;
            world_object.properties = prepared_object.properties;
            world_object.transform = prepared_object.transform;
            world_object.collision = prepared_object.collision;
            world_object.authored_geometry = prepared_object.authored_geometry;
            world_object.trigger = prepared_object.trigger;
            world_object.visibility_region = prepared_object.visibility_region;

            if (prepared_object.tile_object)
            {
                world_object.tile_object = tile_object_component_t{
                    .tilemap = &tilemap,
                    .gid = prepared_object.tile_object->gid,
                    .size_source_px = prepared_object.tile_object->size_source_px,
                    .layer = prepared_object.tile_object->layer,
                    .order_mode = prepared_object.tile_object->order_mode,
                    .order_in_layer = prepared_object.tile_object->order_in_layer,
                    .sort_reference_y = prepared_object.tile_object->sort_reference_y,
                    .sampler_preset = prepared_object.tile_object->sampler_preset,
                    .color = prepared_object.tile_object->color
                };
                ++result.tile_objects_created;
            }
            else
            {
                ++result.markers_created;
            }

            if (world_object.trigger)
                ++result.triggers_created;
        }

        return result;
    }

    tilemap_world_bridge_result_t import_tilemap_objects(world_t& world,
                                                         const assets::loaded_tilemap_asset_t& tilemap,
                                                         const chlm::float2 tilemap_origin_world)
    {
        const prepared_tilemap_world_data_t prepared{
            prepare_tilemap_world_data(tilemap, tilemap_origin_world)
        };
        return apply_prepared_tilemap_world_data(world, tilemap, prepared);
    }
} // namespace carrot::world::import
