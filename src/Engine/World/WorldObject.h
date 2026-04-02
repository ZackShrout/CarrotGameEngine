//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Components/SpriteComponent.h"
#include "Components/TransformComponent.h"

#include <optional>
#include <string>

namespace carrot::world {
    using world_object_id_t = uint64_t;

    class world_object_t
    {
    public:
        world_object_id_t id{ 0 };
        std::string name;
        std::string type;

        std::optional<transform_component_t> transform;
        std::optional<sprite_component_t> sprite;
    };
} // namespace carrot::world
