//
// Created by Zack Shrout on 4/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SceneContinuity.h"
#include "World.h"

namespace carrot::world {
    std::string build_runtime_object_identity(const world_object_t& object)
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

    std::string make_scene_runtime_object_key(const std::string_view scene_id,
                                              const world_object_t& object)
    {
        return std::format("{}::{}", scene_id, build_runtime_object_identity(object));
    }

    std::string make_scene_runtime_object_flag_key(const std::string_view scene_id,
                                                   const world_object_t& object,
                                                   const std::string_view flag_name)
    {
        return std::format("{}::{}", make_scene_runtime_object_key(scene_id, object), flag_name);
    }

    void set_world_object_bool_property(world_object_t& object,
                                        const std::string_view property_name,
                                        const bool value)
    {
        for (assets::tilemap_property_t& property : object.properties)
        {
            if (property.name == property_name)
            {
                property.value = value;
                return;
            }
        }

        object.properties.emplace_back(assets::tilemap_property_t{
            .name = std::string{ property_name },
            .value = value
        });
    }

    void scene_runtime_flag_store_t::mark(const std::string_view scene_id,
                                          const world_object_t& object,
                                          const std::string_view flag_name)
    {
        _flags.emplace(make_scene_runtime_object_flag_key(scene_id, object, flag_name));
    }

    bool scene_runtime_flag_store_t::contains(const std::string_view scene_id,
                                              const world_object_t& object,
                                              const std::string_view flag_name) const
    {
        return _flags.contains(make_scene_runtime_object_flag_key(scene_id, object, flag_name));
    }

    void scene_runtime_flag_store_t::clear() noexcept
    {
        _flags.clear();
    }

    size_t apply_scene_runtime_flag_to_matching_objects(const std::string_view scene_id,
                                                        world_t& world,
                                                        const scene_runtime_flag_store_t& flags,
                                                        const std::string_view flag_name,
                                                        const continuity_object_predicate_t& predicate,
                                                        const continuity_object_callback_t& callback)
    {
        if (!predicate || !callback)
            return 0u;

        size_t applied_count{ 0u };
        for (world_object_t& object : world.objects())
        {
            if (!predicate(object))
                continue;

            if (!flags.contains(scene_id, object, flag_name))
                continue;

            callback(object);
            ++applied_count;
        }

        return applied_count;
    }
} // namespace carrot::world
