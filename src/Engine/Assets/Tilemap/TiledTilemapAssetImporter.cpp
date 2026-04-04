//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TiledTilemapAssetImporter.h"

#include "Assets/AssetID.h"

namespace carrot::assets {
    namespace {
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
            for (const auto& property_value : properties)
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
            for (const auto& property_value : properties)
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
            for (const auto& tile_value : tiles)
            {
                if (!tile_value.is_object())
                    continue;

                const utils::json::json_object_view_t tile_json{ tile_value.as_object() };
                const uint32_t tile_id{ static_cast<uint32_t>(tile_json.get_number_or("id", 0.0)) };

                if (tile_json.has("animation"))
                {
                    diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses animation which is not yet supported",
                                                            tileset_label,
                                                            tile_id));
                }

                if (tile_json.has("objectgroup"))
                {
                    diagnostics.add_unsupported(std::format("tileset '{}' tile {} uses collision/object data which is not yet supported",
                                                            tileset_label,
                                                            tile_id));
                }
            }
        }

        [[nodiscard]] bool parse_tilesets(const utils::json::json_object_view_t& root,
                                          tilemap_asset_t& tilemap,
                                          tiled_import_diagnostics_t& diagnostics)
        {
            if (!root.has("tilesets"))
                return true;

            const utils::json::json_array_view_t tilesets{ root.get_array("tilesets") };
            for (const auto& tileset_value : tilesets)
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
                diagnostics.add_unsupported(std::format("layer '{}' object {} uses polyline geometry which is not yet supported",
                                                        layer_name,
                                                        object_id));
            }

            if (object_json.has("point") && object_json.get_bool_or("point", false))
            {
                diagnostics.add_unsupported(std::format("layer '{}' object {} uses point geometry which is not yet supported",
                                                        layer_name,
                                                        object_id));
            }

            if (object_json.has("text"))
            {
                diagnostics.add_unsupported(std::format("layer '{}' object {} uses text data which is not yet supported",
                                                        layer_name,
                                                        object_id));
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
                    for (const auto& value : data)
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

                    for (const auto& object_value : objects)
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
            const utils::json::json_array_view_t layers{ root.get_array("layers") };
            for (const auto& layer_value : layers)
            {
                if (!layer_value.is_object())
                    continue;

                tilemap_layer_t layer{ };
                if (!parse_layer(layer_value.as_object(), layer, diagnostics))
                    continue;

                record.tilemap.add_layer(std::move(layer));
            }
        }

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register tilemap asset '{}'", logical_id);
            return false;
        }

        for (const std::string& issue : diagnostics.unsupported_features)
            LOG_ASSET_WARN("Tilemap asset '{}': {}", logical_id, issue);

        LOG_ASSET_INFO("Registered Tiled tilemap asset '{}'", logical_id);
        return true;
    }
} // namespace carrot::assets
