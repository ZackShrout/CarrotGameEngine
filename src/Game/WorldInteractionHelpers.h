//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

namespace sandbox {
    enum class interaction_kind_t : uint8_t
    {
        none = 0,
        sign,
        door,
        chest
    };

    struct sign_interaction_data_t
    {
        std::string_view message_id;
    };

    struct door_interaction_data_t
    {
        std::string_view target_scene;
        std::string_view target_map;
        std::string_view target_marker;
    };

    struct scene_transition_request_t
    {
        std::string scene_id;
        std::string marker_name;
    };

    struct chest_interaction_data_t
    {
        std::string_view loot_table;
    };

    struct scene_validation_report_t
    {
        std::vector<std::string> issues;

        [[nodiscard]] bool valid() const noexcept { return issues.empty(); }
    };

    [[nodiscard]] interaction_kind_t interaction_kind_for(const carrot::world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<sign_interaction_data_t> as_sign(const carrot::world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<door_interaction_data_t> as_door(const carrot::world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<chest_interaction_data_t> as_chest(const carrot::world::world_object_t& object) noexcept;
    [[nodiscard]] std::optional<scene_transition_request_t> make_scene_transition_request(
        const carrot::assets::asset_manager_t& assets,
        const carrot::world::world_object_t& object);
    [[nodiscard]] bool validate_scene_transition_target(const carrot::assets::asset_manager_t& assets,
                                                        const carrot::world::world_object_t& object) noexcept;
    [[nodiscard]] bool validate_scene_transition_targets(const carrot::assets::asset_manager_t& assets,
                                                         const carrot::world::world_t& world) noexcept;
    [[nodiscard]] scene_validation_report_t build_scene_validation_report(const carrot::assets::asset_manager_t& assets,
                                                                          const carrot::world::world_t& world) noexcept;
} // namespace sandbox
