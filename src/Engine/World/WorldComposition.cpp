//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "WorldComposition.h"

namespace carrot::world {
    bool authored_world_bounds_t::overlaps(const authored_world_bounds_t& other) const noexcept
    {
        const int64_t left{ x };
        const int64_t top{ y };
        const int64_t right{ left + width };
        const int64_t bottom{ top + height };

        const int64_t other_left{ other.x };
        const int64_t other_top{ other.y };
        const int64_t other_right{ other_left + other.width };
        const int64_t other_bottom{ other_top + other.height };

        return left < other_right && right > other_left && top < other_bottom && bottom > other_top;
    }

    bool authored_world_bounds_t::contains(const int32_t px, const int32_t py) const noexcept
    {
        return px >= x && py >= y && px < (x + width) && py < (y + height);
    }

    std::string runtime_world_chunk_identity_t::stable_id() const
    {
        return std::format("{}#{}@{}", world_source_uri, authored_chunk_index, map_source_uri);
    }

    runtime_world_layout_t runtime_world_layout_t::from_authored_world(const assets::tiled_world_t& world)
    {
        runtime_world_layout_t layout;
        layout._chunks.reserve(world.maps.size());

        for (size_t i{ 0u }; i < world.maps.size(); ++i)
        {
            const assets::tiled_world_map_entry_t& map{ world.maps[i] };
            layout._chunks.push_back({
                .identity = runtime_world_chunk_identity_t{
                    .world_source_uri = world.source_uri,
                    .map_source_uri = map.source_uri,
                    .authored_chunk_index = static_cast<uint32_t>(i)
                },
                .map_file_name = map.file_name,
                .authored_bounds_px = authored_world_bounds_t{
                    .x = map.x,
                    .y = map.y,
                    .width = map.width,
                    .height = map.height
                }
            });
        }

        return layout;
    }

    const runtime_world_chunk_descriptor_t* runtime_world_layout_t::find_by_map_source_uri(
        const std::string_view map_source_uri) const noexcept
    {
        for (const runtime_world_chunk_descriptor_t& chunk : _chunks)
        {
            if (chunk.identity.map_source_uri == map_source_uri)
                return &chunk;
        }

        return nullptr;
    }

    std::vector<const runtime_world_chunk_descriptor_t*> runtime_world_layout_t::query_overlapping(
        const authored_world_bounds_t& bounds) const
    {
        std::vector<const runtime_world_chunk_descriptor_t*> matches;

        for (const runtime_world_chunk_descriptor_t& chunk : _chunks)
        {
            if (chunk.authored_bounds_px.overlaps(bounds))
                matches.push_back(&chunk);
        }

        return matches;
    }

    std::vector<runtime_world_chunk_state_t> runtime_world_layout_t::instantiate_chunk_states() const
    {
        std::vector<runtime_world_chunk_state_t> states;
        states.reserve(_chunks.size());

        for (const runtime_world_chunk_descriptor_t& chunk : _chunks)
        {
            states.push_back({
                .descriptor = chunk
            });
        }

        return states;
    }
} // namespace carrot::world
