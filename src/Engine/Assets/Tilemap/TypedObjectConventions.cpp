//
// Created by zshrout on 4/5/26.
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

        template <typename TObject>
        [[nodiscard]] std::optional<typed_npc_object_t> parse_npc(const TObject& object) noexcept
        {
            if (object.type != "NPC")
                return std::nullopt;

            const auto name{ object.get_string_property("name") };
            if (!name || name->empty())
                return std::nullopt;

            return typed_npc_object_t{
                .name = *name,
                .patrol_path = object.get_string_property("patrol_path").value_or(std::string_view{}),
                .move_speed = [&]() -> std::optional<float>
                {
                    if (const auto move_speed{ object.get_number_property("move_speed") })
                        return std::max(0.f, static_cast<float>(*move_speed));
                    return std::nullopt;
                }(),
                .sprite_id = object.get_string_property("sprite").value_or(std::string_view{})
            };
        }

        template <typename TObject>
        [[nodiscard]] std::optional<typed_patrol_path_object_t> parse_patrol_path(const TObject& object) noexcept
        {
            if (object.type != "PatrolPath")
                return std::nullopt;

            const auto name{ object.get_string_property("name") };
            if (!name || name->empty())
                return std::nullopt;

            const bool ping_pong{ object.get_bool_property("ping_pong").value_or(false) };
            const bool loop{ object.get_bool_property("loop").value_or(!ping_pong) };

            return typed_patrol_path_object_t{
                .name = *name,
                .loop = loop,
                .ping_pong = ping_pong,
                .pause_time = std::max(0.f, static_cast<float>(object.get_number_property("pause_time").value_or(0.0)))
            };
        }

        [[nodiscard]] std::optional<typed_light_kind_t> parse_light_kind(const std::string_view kind) noexcept
        {
            if (kind == "ambient")
                return typed_light_kind_t::ambient;
            if (kind == "point")
                return typed_light_kind_t::point;
            if (kind == "spot")
                return typed_light_kind_t::spot;

            return std::nullopt;
        }

        [[nodiscard]] std::optional<typed_light_behavior_t> parse_light_behavior(
            const std::string_view behavior) noexcept
        {
            if (behavior == "stationary")
                return typed_light_behavior_t::stationary;
            if (behavior == "follow")
                return typed_light_behavior_t::follow;

            return std::nullopt;
        }

        template <typename TObject>
        [[nodiscard]] std::optional<typed_light_object_t> parse_light(const TObject& object) noexcept
        {
            if (object.type != "Light")
                return std::nullopt;

            const auto kind_name{ object.get_string_property("kind") };
            if (!kind_name || kind_name->empty())
                return std::nullopt;

            const auto kind{ parse_light_kind(*kind_name) };
            if (!kind)
                return std::nullopt;

            typed_light_behavior_t behavior{ typed_light_behavior_t::stationary };
            if (const auto behavior_name{ object.get_string_property("behavior") };
                behavior_name && !behavior_name->empty())
            {
                const auto parsed_behavior{ parse_light_behavior(*behavior_name) };
                if (!parsed_behavior)
                    return std::nullopt;

                behavior = *parsed_behavior;
            }

            return typed_light_object_t{
                .kind = *kind,
                .behavior = behavior,
                .color_hex = object.get_string_property("color").value_or(std::string_view{}),
                .intensity = static_cast<float>(object.get_number_property("intensity").value_or(1.0)),
                .radius_world = [&]() -> std::optional<float>
                {
                    if (const auto radius{ object.get_number_property("radius") })
                        return static_cast<float>(*radius);
                    return std::nullopt;
                }(),
                .follow_target = object.get_string_property("follow_target").value_or(std::string_view{})
            };
        }
    } // namespace

    bool is_known_tiled_object_type(const std::string_view type) noexcept
    {
        return type == "Sign" ||
               type == "Door" ||
               type == "Container" ||
               type == "Trigger" ||
               type == "VisibilityZone" ||
               type == "NPC" ||
               type == "PatrolPath" ||
               type == "Light";
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
    std::optional<typed_npc_object_t> as_typed_npc(const tilemap_object_t& object) noexcept { return parse_npc(object); }
    std::optional<typed_npc_object_t> as_typed_npc(const world::world_object_t& object) noexcept { return parse_npc(object); }
    std::optional<typed_patrol_path_object_t> as_typed_patrol_path(const tilemap_object_t& object) noexcept { return parse_patrol_path(object); }
    std::optional<typed_patrol_path_object_t> as_typed_patrol_path(const world::world_object_t& object) noexcept { return parse_patrol_path(object); }
    std::optional<typed_light_object_t> as_typed_light(const tilemap_object_t& object) noexcept { return parse_light(object); }
    std::optional<typed_light_object_t> as_typed_light(const world::world_object_t& object) noexcept { return parse_light(object); }
} // namespace carrot::assets
