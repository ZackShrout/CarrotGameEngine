//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

namespace sandbox {
    [[nodiscard]] carrot::world::world_object_t* find_player(carrot::world::world_t& world) noexcept;
    [[nodiscard]] const carrot::world::world_object_t* find_player(const carrot::world::world_t& world) noexcept;

    [[nodiscard]] carrot::world::world_object_t* find_marker(carrot::world::world_t& world,
                                                             std::string_view marker_name) noexcept;
    [[nodiscard]] const carrot::world::world_object_t* find_marker(const carrot::world::world_t& world,
                                                                   std::string_view marker_name) noexcept;

    [[nodiscard]] carrot::world::world_object_t* find_player_spawn(carrot::world::world_t& world) noexcept;
    [[nodiscard]] const carrot::world::world_object_t* find_player_spawn(const carrot::world::world_t& world) noexcept;
} // namespace sandbox
