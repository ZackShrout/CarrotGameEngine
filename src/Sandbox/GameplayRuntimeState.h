//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "World/SceneContinuity.h"
#include "World/Controllers/PlayerController.h"
#include "World/World.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sandbox {
    struct gameplay_runtime_state_t
    {
        std::optional<carrot::world::facing_direction_t> player_facing;
        carrot::world::scene_runtime_flag_store_t scene_flags;
    };

    struct gameplay_durable_state_t
    {
        std::string scene_id;
        std::string spawn_marker;
        gameplay_runtime_state_t runtime_state;
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
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> serialize_durable_state(
        const gameplay_durable_state_t& durable_state) noexcept;
    [[nodiscard]] std::optional<gameplay_durable_state_t> deserialize_durable_state(
        std::span<const std::uint8_t> bytes) noexcept;
} // namespace sandbox
