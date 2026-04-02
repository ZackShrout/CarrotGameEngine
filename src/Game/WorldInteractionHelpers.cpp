//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "WorldInteractionHelpers.h"

namespace sandbox {
    interaction_kind_t interaction_kind_for(const carrot::world::world_object_t& object) noexcept
    {
        if (object.type == "Sign")
            return interaction_kind_t::sign;

        if (object.type == "Door")
            return interaction_kind_t::door;

        if (object.type == "Chest")
            return interaction_kind_t::chest;

        return interaction_kind_t::none;
    }

    std::optional<sign_interaction_data_t> as_sign(const carrot::world::world_object_t& object) noexcept
    {
        if (interaction_kind_for(object) != interaction_kind_t::sign)
            return std::nullopt;

        const std::optional<std::string_view> message_id{ object.get_string_property("message_id") };
        if (!message_id)
            return std::nullopt;

        return sign_interaction_data_t{
            .message_id = *message_id
        };
    }

    std::optional<door_interaction_data_t> as_door(const carrot::world::world_object_t& object) noexcept
    {
        if (interaction_kind_for(object) != interaction_kind_t::door)
            return std::nullopt;

        const std::optional<std::string_view> target_map{ object.get_string_property("target_map") };
        const std::optional<std::string_view> target_marker{ object.get_string_property("target_marker") };
        if (!target_map || !target_marker)
            return std::nullopt;

        return door_interaction_data_t{
            .target_map = *target_map,
            .target_marker = *target_marker
        };
    }

    std::optional<chest_interaction_data_t> as_chest(const carrot::world::world_object_t& object) noexcept
    {
        if (interaction_kind_for(object) != interaction_kind_t::chest)
            return std::nullopt;

        const std::optional<std::string_view> loot_table{ object.get_string_property("loot_table") };
        if (!loot_table)
            return std::nullopt;

        return chest_interaction_data_t{
            .loot_table = *loot_table
        };
    }
} // namespace sandbox
