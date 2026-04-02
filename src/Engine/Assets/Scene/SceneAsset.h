//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetID.h"

#include <string>

namespace carrot::assets {
    struct scene_asset_t
    {
        std::string tilemap_id;
        std::string player_sprite_id;
        std::string map_object_name{ "OverworldMap" };
        std::string player_spawn_marker{ "PlayerSpawn" };
        std::string player_name{ "Vraden" };
        std::string player_type{ "Character" };
        std::string initial_music_id;
        chlm::float2 presentation_origin_px{ 0.f, 0.f };
        chlm::float2 tilemap_world_position{ 0.f, 0.f };
        float render_pixels_per_unit{ 64.f };
    };

    struct scene_asset_record_t
    {
        asset_id_t id{ 0 };
        std::string logical_id;
        scene_asset_t scene;
    };
} // namespace carrot::assets
