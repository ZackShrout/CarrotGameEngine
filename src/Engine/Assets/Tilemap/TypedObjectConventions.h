//
// Created by zshrout on 4/5/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TilemapAsset.h"

#include <optional>
#include <string_view>

namespace carrot::world {
    class world_object_t;
}

namespace carrot::assets {
    struct typed_sign_object_t
    {
        std::string_view message_id;
    };

    struct typed_door_object_t
    {
        std::string_view target_scene;
        std::string_view target_map;
        std::string_view target_marker;
    };

    struct typed_container_object_t
    {
        std::string_view loot_table;
    };

    struct typed_trigger_object_t
    {
        std::string_view trigger_id;
        std::string_view trigger_kind;
    };

    struct typed_visibility_zone_object_t
    {
        std::string_view visibility_zone_id;
    };

    struct typed_npc_object_t
    {
        std::string_view name;
        std::string_view patrol_path;
        std::optional<float> move_speed;
        std::string_view sprite_id;
    };

    struct typed_patrol_path_object_t
    {
        std::string_view name;
    };

    enum class typed_light_kind_t : uint8_t
    {
        ambient = 0,
        point,
        spot
    };

    enum class typed_light_behavior_t : uint8_t
    {
        stationary = 0,
        follow
    };

    struct typed_light_object_t
    {
        typed_light_kind_t kind{ typed_light_kind_t::point };
        typed_light_behavior_t behavior{ typed_light_behavior_t::stationary };
        std::string_view color_hex;
        float intensity{ 1.f };
        std::optional<float> radius_world;
        std::string_view follow_target;
    };

    [[nodiscard]] bool is_known_tiled_object_type(std::string_view type) noexcept;
    [[nodiscard]] std::optional<typed_sign_object_t> as_typed_sign(const tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_sign_object_t> as_typed_sign(const world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_door_object_t> as_typed_door(const tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_door_object_t> as_typed_door(const world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_container_object_t> as_typed_container(const tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_container_object_t> as_typed_container(const world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_trigger_object_t> as_typed_trigger(const tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_trigger_object_t> as_typed_trigger(const world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_visibility_zone_object_t> as_typed_visibility_zone(const tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_visibility_zone_object_t> as_typed_visibility_zone(const world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_npc_object_t> as_typed_npc(const tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_npc_object_t> as_typed_npc(const world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_patrol_path_object_t> as_typed_patrol_path(const tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_patrol_path_object_t> as_typed_patrol_path(const world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_light_object_t> as_typed_light(const tilemap_object_t& object) noexcept;
    [[nodiscard]] std::optional<typed_light_object_t> as_typed_light(const world::world_object_t& object) noexcept;
} // namespace carrot::assets
