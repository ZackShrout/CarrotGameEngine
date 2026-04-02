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

    world_object_t* world_t::find_first_object_by_type(const std::string_view type) noexcept
    {
        for (world_object_t& object : _objects)
        {
            if (object.type == type)
                return &object;
        }

        return nullptr;
    }

    const world_object_t* world_t::find_first_object_by_type(const std::string_view type) const noexcept
    {
        for (const world_object_t& object : _objects)
        {
            if (object.type == type)
                return &object;
        }

        return nullptr;
    }

    std::vector<world_object_t*> world_t::find_objects_by_type(const std::string_view type) noexcept
    {
        std::vector<world_object_t*> matches;

        for (world_object_t& object : _objects)
        {
            if (object.type == type)
                matches.push_back(&object);
        }

        return matches;
    }

    std::vector<const world_object_t*> world_t::find_objects_by_type(const std::string_view type) const
    {
        std::vector<const world_object_t*> matches;

        for (const world_object_t& object : _objects)
        {
            if (object.type == type)
                matches.push_back(&object);
        }

        return matches;
    }

    world_object_t* world_t::find_nearest_object_by_type(const std::string_view type,
                                                         const chlm::float2& origin,
                                                         const float max_distance) noexcept
    {
        world_object_t* nearest{ nullptr };
        float nearest_distance_sq{ max_distance > 0.f ? max_distance * max_distance : 0.f };
        bool found_match{ false };

        for (world_object_t& object : _objects)
        {
            if (object.type != type || !object.transform)
                continue;

            const float dx{ object.transform->position.x - origin.x };
            const float dy{ object.transform->position.y - origin.y };
            const float distance_sq{ (dx * dx) + (dy * dy) };

            if (!found_match)
            {
                if (max_distance > 0.f && distance_sq > nearest_distance_sq)
                    continue;

                nearest = &object;
                nearest_distance_sq = distance_sq;
                found_match = true;
                continue;
            }

            if (distance_sq < nearest_distance_sq)
            {
                nearest = &object;
                nearest_distance_sq = distance_sq;
            }
        }

        return nearest;
    }

    const world_object_t* world_t::find_nearest_object_by_type(const std::string_view type,
                                                               const chlm::float2& origin,
                                                               const float max_distance) const noexcept
    {
        const world_object_t* nearest{ nullptr };
        float nearest_distance_sq{ max_distance > 0.f ? max_distance * max_distance : 0.f };
        bool found_match{ false };

        for (const world_object_t& object : _objects)
        {
            if (object.type != type || !object.transform)
                continue;

            const float dx{ object.transform->position.x - origin.x };
            const float dy{ object.transform->position.y - origin.y };
            const float distance_sq{ (dx * dx) + (dy * dy) };

            if (!found_match)
            {
                if (max_distance > 0.f && distance_sq > nearest_distance_sq)
                    continue;

                nearest = &object;
                nearest_distance_sq = distance_sq;
                found_match = true;
                continue;
            }

            if (distance_sq < nearest_distance_sq)
            {
                nearest = &object;
                nearest_distance_sq = distance_sq;
            }
        }

        return nearest;
    }

    void world_t::clear() noexcept
    {
        _next_id = 1;
        _objects.clear();
        _presentation = world_presentation_t{ };
    }

    void world_t::update(const float delta_time) noexcept
    {
        for (world_object_t& object : _objects)
        {
            if (object.sprite_animator)
                object.sprite_animator->animator.update(delta_time);
        }
    }
} // namespace carrot::world
