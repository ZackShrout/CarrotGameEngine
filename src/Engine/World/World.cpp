//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "World.h"

namespace carrot::world {
    world_object_t& world_t::create_object()
    {
        world_object_t& object{ _objects.emplace_back() };
        object.id = _next_id++;
        return object;
    }

    world_object_t* world_t::find_object_by_name(const std::string_view name) noexcept
    {
        for (world_object_t& object : _objects)
        {
            if (object.name == name)
                return &object;
        }

        return nullptr;
    }

    const world_object_t* world_t::find_object_by_name(const std::string_view name) const noexcept
    {
        for (const world_object_t& object : _objects)
        {
            if (object.name == name)
                return &object;
        }

        return nullptr;
    }
} // namespace carrot::world
