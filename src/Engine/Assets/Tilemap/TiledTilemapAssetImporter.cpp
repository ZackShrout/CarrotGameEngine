//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TiledTilemapAssetImporter.h"

#include "Assets/AssetID.h"
#include "TilemapValidation.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] std::vector<chlm::float2> parse_point_array(const utils::json::json_object_view_t& object_json,
                                                                  const std::string_view key)
        {
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
        }

        [[nodiscard]] bool is_convex_polygon(const std::vector<chlm::float2>& points) noexcept
        {
            if (points.size() < 3u)
                return false;

            float sign{ 0.f };
            for (size_t i{ 0u }; i < points.size(); ++i)
            {
                const chlm::float2 a{ points[i] };
                const chlm::float2 b{ points[(i + 1u) % points.size()] };
                const chlm::float2 c{ points[(i + 2u) % points.size()] };
                const chlm::float2 ab{ b - a };
                const chlm::float2 bc{ c - b };
                const float cross{ (ab.x * bc.y) - (ab.y * bc.x) };
                if (std::fabs(cross) <= 1.0e-5f)
                    continue;

                if (sign == 0.f)
                {
                    sign = cross;
                    continue;
                }

                if ((cross > 0.f) != (sign > 0.f))
                    return false;
            }

            return sign != 0.f;
        }

        [[nodiscard]] std::vector<chlm::float2> approximate_ellipse_points(const float x,
                                                                            const float y,
                                                                            const float width,
                                                                            const float height) noexcept
        {
            constexpr size_t k_segments{ 12u };
            constexpr float k_pi{ 3.14159265358979323846f };
            std::vector<chlm::float2> points;
            points.reserve(k_segments);

            const chlm::float2 center{ x + (width * 0.5f), y + (height * 0.5f) };
            const float radius_x{ width * 0.5f };
            const float radius_y{ height * 0.5f };
            for (size_t i{ 0u }; i < k_segments; ++i)
            {
                const float angle{
                    (static_cast<float>(i) / static_cast<float>(k_segments)) * k_pi * 2.f
                };
                points.emplace_back(chlm::float2{
                    center.x + (std::cos(angle) * radius_x),
                    center.y + (std::sin(angle) * radius_y)
                });
            }

            return points;
        }

        void merge_inherited_properties(std::vector<tilemap_property_t>& target,
                                        const std::vector<tilemap_property_t>& inherited_properties)
        {
            for (const tilemap_property_t& inherited : inherited_properties)
            {
                if (find_tilemap_property(target, inherited.name) != nullptr)
                    continue;

                target.push_back(inherited);
            }
        }

        struct tiled_import_diagnostics_t
        {
            std::vector<std::string> unsupported_features;

            void add_unsupported(std::string message)
            {
                unsupported_features.emplace_back(std::move(message));
            }
        };

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

        void parse_properties(const utils::json::json_object_view_t& obj, std::vector<tilemap_property_t>& out_properties)
        {
            if (!obj.has("properties"))
                return;

            const utils::json::json_array_view_t properties{ obj.get_array("properties") };
            for (const auto property_value : properties)
            {
                if (!property_value.is_object())
                    continue;

                if (std::optional<tilemap_property_t> property{ parse_property(property_value.as_object()) })
                    out_properties.emplace_back(std::move(*property));
            }
        }

        void parse_tilemap_properties(const utils::json::json_object_view_t& obj, tilemap_asset_t& tilemap)
        {
            if (!obj.has("properties"))
                return;

            const utils::json::json_array_view_t properties{ obj.get_array("properties") };
            for (const auto property_value : properties)
            {
                if (!property_value.is_object())
                    continue;

                if (std::optional<tilemap_property_t> property{ parse_property(property_value.as_object()) })
                    tilemap.add_property(std::move(*property));
            }
        }

        void collect_unsupported_tileset_features(const utils::json::json_object_view_t& tileset_json,
                                                  tiled_import_diagnostics_t& diagnostics)
        {
            const std::string_view tileset_name{ tileset_json.get_string_or("name", "") };
            const std::string_view tileset_label{ tileset_name.empty() ? "<unnamed>" : tileset_name };

            if (tileset_json.has("tileoffset"))
            {
                diagnostics.add_unsupported(std::format("tileset '{}' uses 'tileoffset' which is not yet supported",
                                                        tileset_label));
            }

            if (!tileset_json.has("tiles"))
                return;

            const utils::json::json_array_view_t tiles{ tileset_json.get_array("tiles") };
            for (const auto tile_value : tiles)
            {
                if (!tile_value.is_object())
                    continue;

            }
        }

        void parse_tileset_tile_animation(const utils::json::json_object_view_t& tile_json,
                                          tilemap_tileset_t& tileset,
                                          tiled_import_diagnostics_t& diagnostics)
        {
            if (!tile_json.has("animation"))
                return;

            const uint32_t tile_id{ static_cast<uint32_t>(tile_json.get_number_or("id", 0.0)) };
            const utils::json::json_array_view_t animation_frames{ tile_json.get_array("animation") };

            tilemap_tileset_t::tile_animation_t animation{ };
            animation.tile_id = tile_id;
            animation.frames.reserve(animation_frames.size());

            for (const auto frame_value : animation_frames)
            {
                if (!frame_value.is_object())
                    continue;

                const utils::json::json_object_view_t frame_json{ frame_value.as_object() };
                const uint32_t frame_tile_id{ static_cast<uint32_t>(frame_json.get_number_or("tileid", 0.0)) };
                const uint32_t duration_ms{ static_cast<uint32_t>(frame_json.get_number_or("duration", 0.0)) };

                if (duration_ms == 0)
                {
                    diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses an animation frame with zero duration which is ignored",
                                                            tileset.name.empty() ? "<unnamed>" : tileset.name,
                                                            tile_id));
                    continue;
                }

                animation.frames.push_back({
                    .tile_id = frame_tile_id,
                    .duration_ms = duration_ms
                });
            }

            if (!animation.frames.empty())
                tileset.tile_animations.emplace_back(std::move(animation));
        }

        void collect_unsupported_tileset_collision_object_features(const utils::json::json_object_view_t& object_json,
                                                                   const std::string_view tileset_name,
                                                                   const uint32_t tile_id,
                                                                   tiled_import_diagnostics_t& diagnostics)
        {
            if (object_json.has("polygon"))
            {
                const auto points{ parse_point_array(object_json, "polygon") };
                if (points.size() < 3u)
                {
                    diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses polygon collision geometry with fewer than 3 points",
                                                            tileset_name,
                                                            tile_id));
                }
                else if (!is_convex_polygon(points))
                {
                    diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses concave polygon collision geometry which is not yet supported",
                                                            tileset_name,
                                                            tile_id));
                }
            }

            if (object_json.has("polyline"))
            {
                diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses polyline collision geometry which is not yet supported",
                                                        tileset_name,
                                                        tile_id));
            }

            if (object_json.has("point") && object_json.get_bool_or("point", false))
            {
                diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses point collision geometry which is not yet supported",
                                                        tileset_name,
                                                        tile_id));
            }

            if (object_json.has("text"))
            {
                diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses text collision data which is not yet supported",
                                                        tileset_name,
                                                        tile_id));
            }

            const float width{ static_cast<float>(object_json.get_number_or("width", 0.0)) };
            const float height{ static_cast<float>(object_json.get_number_or("height", 0.0)) };
            if ((object_json.has("ellipse") || !object_json.has("polygon")) &&
                (width <= 0.f || height <= 0.f))
            {
                diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses zero-size collision geometry which is not yet supported",
                                                        tileset_name,
                                                        tile_id));
            }
        }

        void parse_tileset_tile_collision(const utils::json::json_object_view_t& tile_json,
                                          const std::string_view tileset_name,
                                          tilemap_tileset_t& tileset,
                                          tiled_import_diagnostics_t& diagnostics)
        {
            if (!tile_json.has("objectgroup"))
                return;

            const uint32_t tile_id{ static_cast<uint32_t>(tile_json.get_number_or("id", 0.0)) };
            const utils::json::json_object_view_t object_group{ tile_json.get_object("objectgroup") };

            if (!object_group.has("objects"))
                return;

            tilemap_tileset_t::tile_collision_t tile_collision{ };
            tile_collision.tile_id = tile_id;

            const utils::json::json_array_view_t objects{ object_group.get_array("objects") };
            for (const auto object_value : objects)
            {
                if (!object_value.is_object())
                    continue;

                const utils::json::json_object_view_t object_json{ object_value.as_object() };
                collect_unsupported_tileset_collision_object_features(object_json, tileset_name, tile_id, diagnostics);

                const float width{ static_cast<float>(object_json.get_number_or("width", 0.0)) };
                const float height{ static_cast<float>(object_json.get_number_or("height", 0.0)) };
                const float x{ static_cast<float>(object_json.get_number_or("x", 0.0)) };
                const float y{ static_cast<float>(object_json.get_number_or("y", 0.0)) };

                if (object_json.has("polyline") ||
                    object_json.has("text") ||
                    (object_json.has("point") && object_json.get_bool_or("point", false)))
                {
                    continue;
                }

                if (object_json.has("ellipse"))
                {
                    if (width <= 0.f || height <= 0.f)
                        continue;

                    tile_collision.collision_polygons.emplace_back(tilemap_tileset_t::collision_polygon_t{
                        .points = approximate_ellipse_points(x, y, width, height)
                    });
                    continue;
                }

                if (object_json.has("polygon"))
                {
                    std::vector<chlm::float2> points{ parse_point_array(object_json, "polygon") };
                    if (points.size() < 3u || !is_convex_polygon(points))
                        continue;

                    for (chlm::float2& point : points)
                        point += chlm::float2{ x, y };

                    tile_collision.collision_polygons.emplace_back(tilemap_tileset_t::collision_polygon_t{
                        .points = std::move(points)
                    });
                    continue;
                }

                if (width <= 0.f || height <= 0.f)
                    continue;

                tile_collision.collision_rects.emplace_back(tilemap_tileset_t::collision_rect_t{
                    .x = x,
                    .y = y,
                    .width = width,
                    .height = height
                });
            }

            if (!tile_collision.empty())
                tileset.tile_collisions.emplace_back(std::move(tile_collision));
        }

        [[nodiscard]] bool parse_tilesets(const utils::json::json_object_view_t& root,
                                          tilemap_asset_t& tilemap,
                                          tiled_import_diagnostics_t& diagnostics)
        {
            if (!root.has("tilesets"))
                return true;

            const utils::json::json_array_view_t tilesets{ root.get_array("tilesets") };
            for (const auto tileset_value : tilesets)
            {
                if (!tileset_value.is_object())
                    continue;

                const utils::json::json_object_view_t tileset_json{ tileset_value.as_object() };

                tilemap_tileset_t tileset{ };
                tileset.first_gid = static_cast<uint32_t>(tileset_json.get_number_or("firstgid", 0.0));
                tileset.name = std::string{ tileset_json.get_string_or("name", "") };
                tileset.source_uri = std::string{ tileset_json.get_string_or("source", "") };
                tileset.image_source_uri = std::string{ tileset_json.get_string_or("image", "") };
                tileset.tile_width = static_cast<uint32_t>(tileset_json.get_number_or("tilewidth", 0.0));
                tileset.tile_height = static_cast<uint32_t>(tileset_json.get_number_or("tileheight", 0.0));
                tileset.image_width = static_cast<uint32_t>(tileset_json.get_number_or("imagewidth", 0.0));
                tileset.image_height = static_cast<uint32_t>(tileset_json.get_number_or("imageheight", 0.0));
                tileset.tile_count = static_cast<uint32_t>(tileset_json.get_number_or("tilecount", 0.0));
                tileset.columns = static_cast<uint32_t>(tileset_json.get_number_or("columns", 0.0));

                collect_unsupported_tileset_features(tileset_json, diagnostics);
                if (tileset_json.has("tiles"))
                {
                    const utils::json::json_array_view_t tiles{ tileset_json.get_array("tiles") };
                    for (const auto tile_value : tiles)
                    {
                        if (!tile_value.is_object())
                            continue;

                        parse_tileset_tile_animation(tile_value.as_object(), tileset, diagnostics);
                        parse_tileset_tile_collision(tile_value.as_object(), tileset.name, tileset, diagnostics);
                    }
                }
                tileset.rebuild_animation_lookup();
                tilemap.add_tileset(std::move(tileset));
            }

            return true;
        }

        void collect_unsupported_object_features(const utils::json::json_object_view_t& object_json,
                                                 const std::string_view layer_name,
                                                 tiled_import_diagnostics_t& diagnostics)
        {
            const uint32_t object_id{ static_cast<uint32_t>(object_json.get_number_or("id", 0.0)) };

            if (object_json.has("ellipse"))
            {
                diagnostics.add_unsupported(std::format("layer '{}' object {} uses ellipse geometry which is not yet supported",
                                                        layer_name,
                                                        object_id));
            }

            if (object_json.has("polygon"))
            {
                diagnostics.add_unsupported(std::format("layer '{}' object {} uses polygon geometry which is not yet supported",
                                                        layer_name,
                                                        object_id));
            }

            if (object_json.has("polyline"))
            {
                const std::string_view object_type{ object_json.get_string_or("type", "") };
                if (object_type != "PatrolPath")
                {
                    diagnostics.add_unsupported(std::format("layer '{}' object {} uses polyline geometry which is not yet supported",
                                                            layer_name,
                                                            object_id));
                }
            }

            if (object_json.has("text"))
            {
                diagnostics.add_unsupported(std::format("layer '{}' object {} uses text data which is not yet supported",
                                                        layer_name,
                                                        object_id));
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

        [[nodiscard]] bool parse_layer(const utils::json::json_object_view_t& layer_json,
                                       tilemap_layer_t& out_layer,
                                       tiled_import_diagnostics_t& diagnostics)
        {
            const std::string_view type{ layer_json.get_string("type") };
            if (!type.data())
                return false;

            out_layer.name = std::string{ layer_json.get_string_or("name", "") };
            out_layer.authored_type = std::string{ layer_json.get_string_or("class", "") };
            out_layer.visible = layer_json.get_bool_or("visible", true);
            out_layer.opacity = static_cast<float>(layer_json.get_number_or("opacity", 1.0));
            parse_properties(layer_json, out_layer.properties);

            if (type == "tilelayer")
            {
                out_layer.kind = tilemap_layer_kind_t::tile;
                out_layer.width = static_cast<uint32_t>(layer_json.get_number_or("width", 0.0));
                out_layer.height = static_cast<uint32_t>(layer_json.get_number_or("height", 0.0));

                if (layer_json.has("data"))
                {
                    const utils::json::json_array_view_t data{ layer_json.get_array("data") };
                    out_layer.gids.reserve(data.size());
                    for (const auto value : data)
                        out_layer.gids.emplace_back(static_cast<uint32_t>(value.as_number_or(0.0)));
                }

                return true;
            }

            if (type == "objectgroup")
            {
                out_layer.kind = tilemap_layer_kind_t::object;

                if (layer_json.has("objects"))
                {
                    const utils::json::json_array_view_t objects{ layer_json.get_array("objects") };
                    out_layer.objects.reserve(objects.size());

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
                        parse_properties(object_json, object.properties);
                        collect_unsupported_object_features(object_json, out_layer.name, diagnostics);

                        out_layer.objects.emplace_back(std::move(object));
                    }
                }

                return true;
            }

            diagnostics.add_unsupported(std::format("layer '{}' uses unsupported Tiled layer type '{}'",
                                                    out_layer.name,
                                                    type));
            return false;
        }

        constexpr size_t k_max_supported_group_depth{ 32u };

        void parse_layer_list(const utils::json::json_array_view_t& layers,
                              tilemap_asset_t& tilemap,
                              tiled_import_diagnostics_t& diagnostics,
                              const std::vector<tilemap_property_t>& inherited_properties = {},
                              const bool inherited_visible = true,
                              const size_t group_depth = 0u)
        {
            if (group_depth > k_max_supported_group_depth)
            {
                diagnostics.add_unsupported(std::format("tilemap uses nested group depth beyond supported limit ({})",
                                                        k_max_supported_group_depth));
                return;
            }

            for (const auto layer_value : layers)
            {
                if (!layer_value.is_object())
                    continue;

                const utils::json::json_object_view_t layer_json{ layer_value.as_object() };
                const std::string_view type{ layer_json.get_string_or("type", "") };
                const bool effective_visible{ inherited_visible && layer_json.get_bool_or("visible", true) };

                if (type == "group")
                {
                    std::vector<tilemap_property_t> next_inherited_properties{ inherited_properties };
                    parse_properties(layer_json, next_inherited_properties);

                    if (layer_json.has("layers"))
                    {
                        parse_layer_list(layer_json.get_array("layers"),
                                         tilemap,
                                         diagnostics,
                                         next_inherited_properties,
                                         effective_visible,
                                         group_depth + 1u);
                    }
                    continue;
                }

                tilemap_layer_t layer{ };
                if (!parse_layer(layer_json, layer, diagnostics))
                    continue;

                merge_inherited_properties(layer.properties, inherited_properties);
                layer.visible = layer.visible && effective_visible;
                tilemap.add_layer(std::move(layer));
            }
        }
    } // anonymous namespace

    bool tiled_tilemap_asset_importer_t::import(const utils::json::json_document_t& doc,
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
        const std::string_view orientation{ root.get_string_or("orientation", "orthogonal") };
        if (orientation != "orthogonal")
        {
            LOG_ASSET_ERROR("Tilemap asset '{}' currently only supports orthogonal Tiled maps (got '{}')",
                            logical_id, orientation);
            return false;
        }

        record.tilemap.set_orientation(tilemap_orientation_t::orthogonal);
        record.tilemap.set_source_format("tiled.tmj");
        record.tilemap.set_size(
            static_cast<uint32_t>(root.get_number_or("width", 0.0)),
            static_cast<uint32_t>(root.get_number_or("height", 0.0))
        );
        record.tilemap.set_tile_size(
            static_cast<uint32_t>(root.get_number_or("tilewidth", 0.0)),
            static_cast<uint32_t>(root.get_number_or("tileheight", 0.0))
        );
        parse_tilemap_properties(root, record.tilemap);

        tiled_import_diagnostics_t diagnostics;

        if (root.get_bool_or("infinite", false))
        {
            diagnostics.add_unsupported(std::format("tilemap '{}' uses infinite map mode which is not yet supported",
                                                    logical_id));
        }

        if (!parse_tilesets(root, record.tilemap, diagnostics))
            return false;

        if (root.has("layers"))
        {
            parse_layer_list(root.get_array("layers"), record.tilemap, diagnostics);
        }

        for (tilemap_validation_issue_t issue : validate_tiled_authored_data(record.tilemap))
        {
            record.tilemap.add_validation_issue(issue);
        }

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register tilemap asset '{}'", logical_id);
            return false;
        }

        for (const std::string& issue : diagnostics.unsupported_features)
            LOG_ASSET_WARN("Tilemap asset '{}': {}", logical_id, issue);

        if (const tilemap_asset_record_t* stored_record{ registry.find(make_asset_id(logical_id)) })
        {
            for (const tilemap_validation_issue_t& issue : stored_record->tilemap.validation_issues())
            {
                const std::string_view severity{
                    issue.severity == tilemap_validation_issue_severity_t::error ? "error" : "warning"
                };
                LOG_ASSET_WARN("Tilemap asset '{}': [{}] {} ({})",
                               logical_id,
                               severity,
                               issue.message,
                               issue.code);
            }
        }

        LOG_ASSET_INFO("Registered Tiled tilemap asset '{}'", logical_id);
        return true;
    }
} // namespace carrot::assets
