//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetID.h"

#include <cstdint>
#include <string>

namespace carrot::assets {
    enum class scene_camera_follow_mode_t : uint8_t
    {
        none = 0,
        player
    };

    enum class scene_camera_initial_target_policy_t : uint8_t
    {
        player = 0,
        spawn_marker
    };

    struct scene_camera_defaults_t
    {
        float zoom{ 4.f };
        scene_camera_follow_mode_t follow_mode{ scene_camera_follow_mode_t::player };
        scene_camera_initial_target_policy_t initial_target_policy{
            scene_camera_initial_target_policy_t::player
        };
        chlm::float2 dead_zone_size_world{ 2.0f, 1.5f };
        float follow_smoothing{ 10.0f };
    };

    struct scene_asset_t
    {
        std::string tilemap_id;
        std::string player_sprite_id;
        std::string map_object_name{ "OverworldMap" };
        std::string player_spawn_marker{ "PlayerSpawn" };
        std::string player_name{ "Vraden" };
        std::string player_type{ "Character" };
        std::string initial_music_id;
        scene_camera_defaults_t camera{ };
        chlm::float2 presentation_origin_px{ 0.f, 0.f };
        chlm::float2 tilemap_world_position{ 0.f, 0.f };
    };

    struct scene_asset_record_t
    {
        asset_id_t id{ 0 };
        std::string logical_id;
        std::string source_uri;
        scene_asset_t scene;
    };
} // namespace carrot::assets
