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
        [[nodiscard]] bool validate_non_empty_field(const std::string_view scene_id,
                                                    const std::string_view field_name,
                                                    const std::string_view value)
        {
            if (!value.empty())
                return true;

            LOG_ASSET_ERROR("Scene asset '{}' has an empty '{}' field", scene_id, field_name);
            return false;
        }

        [[nodiscard]] scene_camera_follow_mode_t parse_camera_follow_mode(const std::string_view scene_id,
                                                                          const std::string_view value,
                                                                          const scene_camera_follow_mode_t fallback)
        {
            if (value.empty() || value == "player")
                return scene_camera_follow_mode_t::player;

            if (value == "none")
                return scene_camera_follow_mode_t::none;

            LOG_ASSET_WARN("Scene asset '{}' uses unknown camera.follow_mode '{}'; falling back to '{}'",
                           scene_id,
                           value,
                           fallback == scene_camera_follow_mode_t::player ? "player" : "none");
            return fallback;
        }

        [[nodiscard]] scene_camera_initial_target_policy_t parse_camera_initial_target_policy(
            const std::string_view scene_id,
            const std::string_view value,
            const scene_camera_initial_target_policy_t fallback)
        {
            if (value.empty() || value == "player")
                return scene_camera_initial_target_policy_t::player;

            if (value == "spawn_marker")
                return scene_camera_initial_target_policy_t::spawn_marker;

            LOG_ASSET_WARN("Scene asset '{}' uses unknown camera.initial_target '{}'; falling back to '{}'",
                           scene_id,
                           value,
                           fallback == scene_camera_initial_target_policy_t::spawn_marker ? "spawn_marker" : "player");
            return fallback;
        }

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
                                                 [[maybe_unused]] const io::virtual_file_system_t& vfs,
                                                 const std::string_view manifest_uri)
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
        record.source_uri = std::string{ manifest_uri };

        if (registry.contains(record.id))
        {
            LOG_ASSET_ERROR("Duplicate scene asset id '{}'", id);
            return false;
        }

        record.scene.tilemap_id = std::string{ tilemap };
        record.scene.player_sprite_id = std::string{ player_sprite };

        if (root.has("map_object_name"))
        {
            record.scene.map_object_name = std::string{ root.get_string("map_object_name") };
            if (!validate_non_empty_field(id, "map_object_name", record.scene.map_object_name))
                return false;
        }

        if (root.has("player_spawn_marker"))
        {
            record.scene.player_spawn_marker = std::string{ root.get_string("player_spawn_marker") };
            if (!validate_non_empty_field(id, "player_spawn_marker", record.scene.player_spawn_marker))
                return false;
        }

        if (root.has("player_name"))
        {
            record.scene.player_name = std::string{ root.get_string("player_name") };
            if (!validate_non_empty_field(id, "player_name", record.scene.player_name))
                return false;
        }

        if (root.has("player_type"))
        {
            record.scene.player_type = std::string{ root.get_string("player_type") };
            if (!validate_non_empty_field(id, "player_type", record.scene.player_type))
                return false;
        }

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

        if (root.has("camera"))
        {
            const utils::json::json_object_view_t camera{ root.get_object("camera") };
            const float zoom{ static_cast<float>(camera.get_number_or("zoom", record.scene.camera.zoom)) };
            record.scene.camera.zoom = zoom > 0.f ? zoom : record.scene.camera.zoom;

            if (camera.has("follow_mode"))
            {
                record.scene.camera.follow_mode = parse_camera_follow_mode(
                    id,
                    camera.get_string_or("follow_mode", "player"),
                    record.scene.camera.follow_mode);
            }
            else if (camera.has("follow_player"))
            {
                const bool follow_player{ camera.get_bool_or("follow_player", true) };
                record.scene.camera.follow_mode = follow_player
                    ? scene_camera_follow_mode_t::player
                    : scene_camera_follow_mode_t::none;
            }

            if (camera.has("initial_target"))
            {
                record.scene.camera.initial_target_policy = parse_camera_initial_target_policy(
                    id,
                    camera.get_string_or("initial_target", "player"),
                    record.scene.camera.initial_target_policy);
            }

            [[maybe_unused]] const bool has_dead_zone_size{
                read_float2(camera, "dead_zone_world_size", record.scene.camera.dead_zone_size_world)
            };

            const float follow_smoothing{
                static_cast<float>(camera.get_number_or("follow_smoothing", record.scene.camera.follow_smoothing))
            };
            record.scene.camera.follow_smoothing = follow_smoothing >= 0.f
                ? follow_smoothing
                : record.scene.camera.follow_smoothing;
        }

        if (root.has("render_pixels_per_unit") || root.has("pixels_per_unit"))
        {
            LOG_ASSET_WARN(
                "Scene asset '{}' uses deprecated render-scale fields. Use camera.zoom instead; scene world sizing now stays engine-owned.",
                id
            );
        }

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
