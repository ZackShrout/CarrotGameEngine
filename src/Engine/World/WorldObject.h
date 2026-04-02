//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Components/SpriteAnimatorComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TileObjectComponent.h"
#include "Components/TilemapComponent.h"
#include "Components/TransformComponent.h"
#include "Assets/Tilemap/TilemapAsset.h"

#include <optional>
#include <string>
#include <vector>

namespace carrot::world {
    using world_object_id_t = uint64_t;

    struct world_object_source_t
    {
        std::string tilemap_logical_id;
        std::string layer_name;
        uint32_t object_id{ 0 };
        std::string object_name;
    };

    class world_object_t
    {
    public:
        world_object_id_t id{ 0 };
        std::string name;
        std::string type;
        std::optional<world_object_source_t> source;
        std::vector<assets::tilemap_property_t> properties;

        std::optional<transform_component_t> transform;
        std::optional<sprite_component_t> sprite;
        std::optional<sprite_animator_component_t> sprite_animator;
        std::optional<tile_object_component_t> tile_object;
        std::optional<tilemap_component_t> tilemap;
    };
} // namespace carrot::world
