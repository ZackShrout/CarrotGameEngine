//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "World/SceneContinuity.h"
#include "World/Controllers/PlayerController.h"
#include "World/World.h"

#include <optional>
#include <string_view>

namespace sandbox {
    struct gameplay_runtime_state_t
    {
        std::optional<carrot::world::facing_direction_t> player_facing;
        carrot::world::scene_runtime_flag_store_t scene_flags;
    };

    void capture_player_runtime_state(gameplay_runtime_state_t& runtime_state,
                                      const carrot::world::player_controller_t& player_controller) noexcept;
    void apply_runtime_state_to_player(const gameplay_runtime_state_t& runtime_state,
                                       carrot::world::player_controller_t& player_controller) noexcept;
    void mark_container_open(gameplay_runtime_state_t& runtime_state,
                             std::string_view scene_id,
                             const carrot::world::world_object_t& container);
    [[nodiscard]] bool is_container_open(const gameplay_runtime_state_t& runtime_state,
                                         std::string_view scene_id,
                                         const carrot::world::world_object_t& container);
    void apply_runtime_state_to_scene(std::string_view scene_id,
                                      carrot::world::world_t& world,
                                      const gameplay_runtime_state_t& runtime_state);
} // namespace sandbox
