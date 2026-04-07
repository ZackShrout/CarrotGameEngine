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
} // namespace carrot::assets
