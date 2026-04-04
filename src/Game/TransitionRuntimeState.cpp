//
// Created by Codex on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TransitionRuntimeState.h"

namespace sandbox {
    namespace {
        void set_bool_property(carrot::world::world_object_t& object,
                               const std::string_view property_name,
                               const bool value)
        {
            for (carrot::assets::tilemap_property_t& property : object.properties)
            {
                if (property.name == property_name)
                {
                    property.value = value;
                    return;
                }
            }

            object.properties.emplace_back(carrot::assets::tilemap_property_t{
                .name = std::string{ property_name },
                .value = value
            });
        }

        [[nodiscard]] std::string build_object_identity(const carrot::world::world_object_t& object)
        {
            if (!object.name.empty())
                return std::format("name:{}", object.name);

            if (object.source)
            {
                return std::format("source:{}:{}:{}",
                                   object.source->tilemap_logical_id,
                                   object.source->layer_name,
                                   object.source->object_id);
            }

            return std::format("runtime:{}", object.id);
        }
    } // namespace

    std::string make_scene_runtime_object_key(const std::string_view scene_id,
                                              const carrot::world::world_object_t& object)
    {
        return std::format("{}::{}", scene_id, build_object_identity(object));
    }

    void capture_player_runtime_state(gameplay_runtime_state_t& runtime_state,
                                      const carrot::world::player_controller_t& player_controller) noexcept
    {
        if (!player_controller.controlled_object())
            return;

        runtime_state.player_facing = player_controller.facing_direction();
    }

    void apply_runtime_state_to_player(const gameplay_runtime_state_t& runtime_state,
                                       carrot::world::player_controller_t& player_controller) noexcept
    {
        if (!runtime_state.player_facing)
            return;

        player_controller.set_facing_direction(*runtime_state.player_facing);
    }

    void mark_chest_open(gameplay_runtime_state_t& runtime_state,
                         const std::string_view scene_id,
                         const carrot::world::world_object_t& chest)
    {
        runtime_state.opened_chests.emplace(make_scene_runtime_object_key(scene_id, chest));
    }

    bool is_chest_open(const gameplay_runtime_state_t& runtime_state,
                       const std::string_view scene_id,
                       const carrot::world::world_object_t& chest)
    {
        return runtime_state.opened_chests.contains(make_scene_runtime_object_key(scene_id, chest));
    }

    void apply_runtime_state_to_scene(const std::string_view scene_id,
                                      carrot::world::world_t& world,
                                      const gameplay_runtime_state_t& runtime_state)
    {
        for (carrot::world::world_object_t* chest : world.find_objects_by_type("Chest"))
        {
            if (!chest)
                continue;

            if (!is_chest_open(runtime_state, scene_id, *chest))
                continue;

            set_bool_property(*chest, "interactable", false);
        }
    }
} // namespace sandbox
