//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "NativeTilemapAssetImporter.h"

#include "Assets/AssetID.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] std::optional<tilemap_property_t> parse_property(const utils::json::json_object_view_t& obj)
        {
            const std::string_view name{ obj.get_string("name") };
            if (!name.data() || name.empty())
                return std::nullopt;

            tilemap_property_t property{ };
            property.name = std::string{ name };

            const utils::json::json_value_view_t value{ obj.get("value") };
            if (value.is_bool())
                property.value = value.as_bool();
            else if (value.is_number())
                property.value = value.as_number();
            else
                property.value = std::string{ value.as_string_or("") };

            return property;
        }

        void parse_properties_array(const utils::json::json_array_view_t& array, std::vector<tilemap_property_t>& out_properties)
        {
            for (const auto value : array)
            {
                if (!value.is_object())
                    continue;

                if (std::optional<tilemap_property_t> property{ parse_property(value.as_object()) })
                    out_properties.emplace_back(std::move(*property));
            }
        }

        void parse_tilemap_properties(const utils::json::json_array_view_t& array, tilemap_asset_t& tilemap)
        {
            for (const auto value : array)
            {
                if (!value.is_object())
                    continue;

                if (std::optional<tilemap_property_t> property{ parse_property(value.as_object()) })
                    tilemap.add_property(std::move(*property));
            }
        }

        void parse_object_geometry(const utils::json::json_object_view_t& object_json, tilemap_object_t& object)
        {
            object.geometry_kind = tilemap_object_t::geometry_kind_t::rectangle;
            object.geometry_points.clear();

            if (object_json.has("point") && object_json.get_bool_or("point", false))
            {
                object.geometry_kind = tilemap_object_t::geometry_kind_t::point;
                return;
            }

            if (object_json.has("ellipse"))
            {
                object.geometry_kind = tilemap_object_t::geometry_kind_t::ellipse;
                return;
            }

            if (object_json.has("text"))
            {
                object.geometry_kind = tilemap_object_t::geometry_kind_t::text;
                return;
            }

            const auto parse_point_array = [&object_json](const std::string_view key) -> std::vector<chlm::float2> {
                std::vector<chlm::float2> points;
                if (!object_json.has(key))
                    return points;

                const utils::json::json_array_view_t point_array{ object_json.get_array(key) };
                points.reserve(point_array.size());

                for (const auto point_value : point_array)
                {
                    if (!point_value.is_object())
                        continue;

                    const utils::json::json_object_view_t point_json{ point_value.as_object() };
                    points.emplace_back(chlm::float2{
                        static_cast<float>(point_json.get_number_or("x", 0.0)),
                        static_cast<float>(point_json.get_number_or("y", 0.0))
                    });
                }

                return points;
            };

            if (object_json.has("polygon"))
            {
                object.geometry_kind = tilemap_object_t::geometry_kind_t::polygon;
                object.geometry_points = parse_point_array("polygon");
                return;
            }

            if (object_json.has("polyline"))
            {
                object.geometry_kind = tilemap_object_t::geometry_kind_t::polyline;
                object.geometry_points = parse_point_array("polyline");
            }
        }
    }

    bool native_tilemap_asset_importer_t::import(const utils::json::json_document_t& doc,
                                                 tilemap_asset_registry_t& registry,
                                                 const std::string_view logical_id,
                                                 const std::string_view source_uri)
    {
        tilemap_asset_record_t record{ };
        record.id = make_asset_id(logical_id);
        record.logical_id = std::string{ logical_id };
        record.source_uri = std::string{ source_uri };

        if (registry.contains(record.id))
        {
            LOG_ASSET_ERROR("Duplicate tilemap asset id '{}'", logical_id);
            return false;
        }

        const utils::json::json_object_view_t root{ doc.root().as_object() };
        record.tilemap.set_orientation(tilemap_orientation_t::orthogonal);
        record.tilemap.set_source_format("carrot.ctilemap.json");
        record.tilemap.set_size(
            static_cast<uint32_t>(root.get_number_or("width", 0.0)),
            static_cast<uint32_t>(root.get_number_or("height", 0.0))
        );
        record.tilemap.set_tile_size(
            static_cast<uint32_t>(root.get_number_or("tile_width", 0.0)),
            static_cast<uint32_t>(root.get_number_or("tile_height", 0.0))
        );

        if (root.has("properties"))
            parse_tilemap_properties(root.get_array("properties"), record.tilemap);

        if (root.has("tilesets"))
        {
            const utils::json::json_array_view_t tilesets{ root.get_array("tilesets") };
            for (const auto value : tilesets)
            {
                if (!value.is_object())
                    continue;

                const utils::json::json_object_view_t obj{ value.as_object() };

                tilemap_tileset_t tileset{ };
                tileset.name = std::string{ obj.get_string_or("name", "") };
                tileset.first_gid = static_cast<uint32_t>(obj.get_number_or("first_gid", 0.0));
                tileset.source_uri = std::string{ obj.get_string_or("source", "") };
                tileset.image_source_uri = std::string{ obj.get_string_or("image", "") };
                tileset.tile_width = static_cast<uint32_t>(obj.get_number_or("tile_width", 0.0));
                tileset.tile_height = static_cast<uint32_t>(obj.get_number_or("tile_height", 0.0));
                tileset.image_width = static_cast<uint32_t>(obj.get_number_or("image_width", 0.0));
                tileset.image_height = static_cast<uint32_t>(obj.get_number_or("image_height", 0.0));
                tileset.tile_count = static_cast<uint32_t>(obj.get_number_or("tile_count", 0.0));
                tileset.columns = static_cast<uint32_t>(obj.get_number_or("columns", 0.0));
                if (obj.has("tiles"))
                {
                    const utils::json::json_array_view_t tiles{ obj.get_array("tiles") };
                    for (const auto tile_value : tiles)
                    {
                        if (!tile_value.is_object())
                            continue;

                        const utils::json::json_object_view_t tile_json{ tile_value.as_object() };
                        tilemap_tileset_t::tile_collision_t tile_collision{ };
                        tile_collision.tile_id = static_cast<uint32_t>(tile_json.get_number_or("tile_id", 0.0));

                        if (tile_json.has("collision_rects"))
                        {
                            const utils::json::json_array_view_t rects{ tile_json.get_array("collision_rects") };
                            for (const auto rect_value : rects)
                            {
                                if (!rect_value.is_object())
                                    continue;

                                const utils::json::json_object_view_t rect_json{ rect_value.as_object() };
                                tile_collision.collision_rects.emplace_back(tilemap_tileset_t::collision_rect_t{
                                    .x = static_cast<float>(rect_json.get_number_or("x", 0.0)),
                                    .y = static_cast<float>(rect_json.get_number_or("y", 0.0)),
                                    .width = static_cast<float>(rect_json.get_number_or("width", 0.0)),
                                    .height = static_cast<float>(rect_json.get_number_or("height", 0.0))
                                });
                            }
                        }

                        if (!tile_collision.collision_rects.empty())
                            tileset.tile_collisions.emplace_back(std::move(tile_collision));
                    }
                }

                record.tilemap.add_tileset(std::move(tileset));
            }
        }

        if (root.has("layers"))
        {
            const utils::json::json_array_view_t layers{ root.get_array("layers") };
            for (const auto value : layers)
            {
                if (!value.is_object())
                    continue;

                const utils::json::json_object_view_t obj{ value.as_object() };
                const std::string_view kind{ obj.get_string_or("kind", "tile") };

                tilemap_layer_t layer{ };
                layer.kind = kind == "object" ? tilemap_layer_kind_t::object : tilemap_layer_kind_t::tile;
                layer.name = std::string{ obj.get_string_or("name", "") };
                layer.width = static_cast<uint32_t>(obj.get_number_or("width", 0.0));
                layer.height = static_cast<uint32_t>(obj.get_number_or("height", 0.0));
                layer.visible = obj.get_bool_or("visible", true);
                layer.opacity = static_cast<float>(obj.get_number_or("opacity", 1.0));

                if (obj.has("properties"))
                    parse_properties_array(obj.get_array("properties"), layer.properties);

                if (layer.kind == tilemap_layer_kind_t::tile && obj.has("gids"))
                {
                    const utils::json::json_array_view_t gids{ obj.get_array("gids") };
                    layer.gids.reserve(gids.size());
                    for (const auto gid_value : gids)
                        layer.gids.emplace_back(static_cast<uint32_t>(gid_value.as_number_or(0.0)));
                }

                if (layer.kind == tilemap_layer_kind_t::object && obj.has("objects"))
                {
                    const utils::json::json_array_view_t objects{ obj.get_array("objects") };
                    layer.objects.reserve(objects.size());

                    for (const auto object_value : objects)
                    {
                        if (!object_value.is_object())
                            continue;

                        const utils::json::json_object_view_t object_json{ object_value.as_object() };

                        tilemap_object_t object{ };
                        object.id = static_cast<uint32_t>(object_json.get_number_or("id", 0.0));
                        object.name = std::string{ object_json.get_string_or("name", "") };
                        object.type = std::string{ object_json.get_string_or("type", "") };
                        object.x = static_cast<float>(object_json.get_number_or("x", 0.0));
                        object.y = static_cast<float>(object_json.get_number_or("y", 0.0));
                        object.width = static_cast<float>(object_json.get_number_or("width", 0.0));
                        object.height = static_cast<float>(object_json.get_number_or("height", 0.0));
                        object.rotation = static_cast<float>(object_json.get_number_or("rotation", 0.0));
                        object.visible = object_json.get_bool_or("visible", true);
                        object.gid = static_cast<uint32_t>(object_json.get_number_or("gid", 0.0));
                        parse_object_geometry(object_json, object);

                        if (object_json.has("properties"))
                            parse_properties_array(object_json.get_array("properties"), object.properties);

                        layer.objects.emplace_back(std::move(object));
                    }
                }

                record.tilemap.add_layer(std::move(layer));
            }
        }

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register tilemap asset '{}'", logical_id);
            return false;
        }

        LOG_ASSET_INFO("Registered native tilemap asset '{}'", logical_id);
        return true;
    }
} // namespace carrot::assets
