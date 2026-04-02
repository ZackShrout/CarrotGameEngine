//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SceneAssetManifestImporter.h"

#include "Assets/AssetID.h"
#include "IO/VirtualFileSystem.h"
#include "SceneAssetRegistry.h"
#include "Utils/JSON/Public/JsonDocument.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] bool read_float2(const utils::json::json_object_view_t& obj,
                                       const char* key,
                                       chlm::float2& out_value)
        {
            if (!obj.has(key))
                return false;

            const utils::json::json_object_view_t value{ obj.get_object(key) };
            out_value = {
                static_cast<float>(value.get_number_or("x", 0.0)),
                static_cast<float>(value.get_number_or("y", 0.0))
            };
            return true;
        }
    } // namespace

    bool scene_asset_manifest_importer_t::import(const utils::json::json_document_t& doc,
                                                 scene_asset_registry_t& registry,
                                                 [[maybe_unused]] const io::virtual_file_system_t& vfs)
    {
        const utils::json::json_object_view_t root{ doc.root().as_object() };

        const std::string_view id{ root.get_string("id") };
        const std::string_view tilemap{ root.get_string("tilemap") };
        const std::string_view player_sprite{ root.get_string("player_sprite") };

        if (!id.data() || !tilemap.data() || !player_sprite.data())
        {
            LOG_ASSET_ERROR("Missing required 'id', 'tilemap', or 'player_sprite'");
            return false;
        }

        if (!is_valid_logical_asset_id(id))
        {
            const std::string suggested{ recommend_logical_asset_id(id) };

            if (!suggested.empty() && suggested != id)
            {
                LOG_ASSET_ERROR(
                    "Invalid logical asset id '{}'. Expected [a-z0-9._]+, no leading/trailing '.', and no consecutive dots. Consider '{}'.",
                    id, suggested
                );
            }
            else
            {
                LOG_ASSET_ERROR(
                    "Invalid logical asset id '{}'. Expected [a-z0-9._]+, no leading/trailing '.', and no consecutive dots.",
                    id
                );
            }

            return false;
        }

        if (!is_valid_logical_asset_id(tilemap))
        {
            LOG_ASSET_ERROR("Scene asset '{}' references invalid tilemap asset id '{}'", id, tilemap);
            return false;
        }

        if (!is_valid_logical_asset_id(player_sprite))
        {
            LOG_ASSET_ERROR("Scene asset '{}' references invalid player sprite asset id '{}'", id, player_sprite);
            return false;
        }

        scene_asset_record_t record{ };
        record.id = make_asset_id(id);
        record.logical_id = std::string{ id };

        if (registry.contains(record.id))
        {
            LOG_ASSET_ERROR("Duplicate scene asset id '{}'", id);
            return false;
        }

        record.scene.tilemap_id = std::string{ tilemap };
        record.scene.player_sprite_id = std::string{ player_sprite };

        if (root.has("map_object_name"))
            record.scene.map_object_name = std::string{ root.get_string("map_object_name") };

        if (root.has("player_spawn_marker"))
            record.scene.player_spawn_marker = std::string{ root.get_string("player_spawn_marker") };

        if (root.has("player_name"))
            record.scene.player_name = std::string{ root.get_string("player_name") };

        if (root.has("player_type"))
            record.scene.player_type = std::string{ root.get_string("player_type") };

        if (root.has("initial_music"))
        {
            const std::string_view initial_music{ root.get_string("initial_music") };
            if (initial_music.data() && !initial_music.empty())
            {
                if (!is_valid_logical_asset_id(initial_music))
                {
                    LOG_ASSET_ERROR("Scene asset '{}' references invalid music asset id '{}'", id, initial_music);
                    return false;
                }

                record.scene.initial_music_id = std::string{ initial_music };
            }
        }

        if (root.has("render_pixels_per_unit"))
            record.scene.render_pixels_per_unit = static_cast<float>(root.get_number_or("render_pixels_per_unit", 64.0));
        else if (root.has("pixels_per_unit"))
            record.scene.render_pixels_per_unit = static_cast<float>(root.get_number_or("pixels_per_unit", 64.0));

        [[maybe_unused]] const bool has_presentation_origin{
            read_float2(root, "presentation_origin_px", record.scene.presentation_origin_px)
        };
        [[maybe_unused]] const bool has_tilemap_world_position{
            read_float2(root, "tilemap_world_position", record.scene.tilemap_world_position)
        };

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register scene asset '{}'", id);
            return false;
        }

        LOG_ASSET_INFO("Registered scene asset '{}'", id);
        return true;
    }
} // namespace carrot::assets
