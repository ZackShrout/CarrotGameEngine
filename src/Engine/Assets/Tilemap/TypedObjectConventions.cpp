//
// Created by Codex on 4/5/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TypedObjectConventions.h"

#include "World/WorldObject.h"

namespace carrot::assets {
    namespace {
        template <typename TObject>
        [[nodiscard]] std::optional<typed_sign_object_t> parse_sign(const TObject& object) noexcept
        {
            if (object.type != "Sign")
                return std::nullopt;

            const auto message_id{ object.get_string_property("message_id") };
            if (!message_id || message_id->empty())
                return std::nullopt;

            return typed_sign_object_t{
                .message_id = *message_id
            };
        }

        template <typename TObject>
        [[nodiscard]] std::optional<typed_door_object_t> parse_door(const TObject& object) noexcept
        {
            if (object.type != "Door")
                return std::nullopt;

            const auto target_scene{ object.get_string_property("target_scene") };
            const auto target_map{ object.get_string_property("target_map") };
            const auto target_marker{ object.get_string_property("target_marker") };
            if ((!target_scene || target_scene->empty()) &&
                (!target_map || target_map->empty()))
            {
                return std::nullopt;
            }

            if (!target_marker || target_marker->empty())
                return std::nullopt;

            return typed_door_object_t{
                .target_scene = target_scene.value_or(std::string_view{}),
                .target_map = target_map.value_or(std::string_view{}),
                .target_marker = *target_marker
            };
        }

        template <typename TObject>
        [[nodiscard]] std::optional<typed_container_object_t> parse_container(const TObject& object) noexcept
        {
            if (object.type != "Container")
                return std::nullopt;

            const auto loot_table{ object.get_string_property("loot_table") };
            if (!loot_table || loot_table->empty())
                return std::nullopt;

            return typed_container_object_t{
                .loot_table = *loot_table
            };
        }

        template <typename TObject>
        [[nodiscard]] std::optional<typed_trigger_object_t> parse_trigger(const TObject& object) noexcept
        {
            if (object.type != "Trigger")
                return std::nullopt;

            const auto trigger_id{ object.get_string_property("trigger_id") };
            const auto trigger_kind{ object.get_string_property("trigger_kind") };
            if (!trigger_id || trigger_id->empty() || !trigger_kind || trigger_kind->empty())
                return std::nullopt;

            return typed_trigger_object_t{
                .trigger_id = *trigger_id,
                .trigger_kind = *trigger_kind
            };
        }

        template <typename TObject>
        [[nodiscard]] std::optional<typed_visibility_zone_object_t> parse_visibility_zone(const TObject& object) noexcept
        {
            if (object.type != "VisibilityZone")
                return std::nullopt;

            const auto visibility_zone_id{ object.get_string_property("visibility_zone_id") };
            if (!visibility_zone_id || visibility_zone_id->empty())
                return std::nullopt;

            return typed_visibility_zone_object_t{
                .visibility_zone_id = *visibility_zone_id
            };
        }
    } // namespace

    bool is_known_tiled_object_type(const std::string_view type) noexcept
    {
        return type == "Sign" ||
               type == "Door" ||
               type == "Container" ||
               type == "Trigger" ||
               type == "VisibilityZone";
    }

    std::optional<typed_sign_object_t> as_typed_sign(const tilemap_object_t& object) noexcept { return parse_sign(object); }
    std::optional<typed_sign_object_t> as_typed_sign(const world::world_object_t& object) noexcept { return parse_sign(object); }
    std::optional<typed_door_object_t> as_typed_door(const tilemap_object_t& object) noexcept { return parse_door(object); }
    std::optional<typed_door_object_t> as_typed_door(const world::world_object_t& object) noexcept { return parse_door(object); }
    std::optional<typed_container_object_t> as_typed_container(const tilemap_object_t& object) noexcept { return parse_container(object); }
    std::optional<typed_container_object_t> as_typed_container(const world::world_object_t& object) noexcept { return parse_container(object); }
    std::optional<typed_trigger_object_t> as_typed_trigger(const tilemap_object_t& object) noexcept { return parse_trigger(object); }
    std::optional<typed_trigger_object_t> as_typed_trigger(const world::world_object_t& object) noexcept { return parse_trigger(object); }
    std::optional<typed_visibility_zone_object_t> as_typed_visibility_zone(const tilemap_object_t& object) noexcept { return parse_visibility_zone(object); }
    std::optional<typed_visibility_zone_object_t> as_typed_visibility_zone(const world::world_object_t& object) noexcept { return parse_visibility_zone(object); }
} // namespace carrot::assets
