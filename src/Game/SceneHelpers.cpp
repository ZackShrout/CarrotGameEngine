//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SceneHelpers.h"

namespace sandbox {
    carrot::world::world_object_t* find_player(carrot::world::world_t& world) noexcept
    {
        return world.find_object_by_name("Vraden");
    }

    const carrot::world::world_object_t* find_player(const carrot::world::world_t& world) noexcept
    {
        return world.find_object_by_name("Vraden");
    }

    carrot::world::world_object_t* find_marker(carrot::world::world_t& world,
                                               const std::string_view marker_name) noexcept
    {
        return world.find_object_by_name(marker_name);
    }

    const carrot::world::world_object_t* find_marker(const carrot::world::world_t& world,
                                                     const std::string_view marker_name) noexcept
    {
        return world.find_object_by_name(marker_name);
    }

    carrot::world::world_object_t* find_player_spawn(carrot::world::world_t& world) noexcept
    {
        return find_marker(world, "PlayerSpawn");
    }

    const carrot::world::world_object_t* find_player_spawn(const carrot::world::world_t& world) noexcept
    {
        return find_marker(world, "PlayerSpawn");
    }
} // namespace sandbox
