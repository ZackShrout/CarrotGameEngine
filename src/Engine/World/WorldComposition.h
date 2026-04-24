//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Tilemap/TiledWorld.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace carrot::world {
    struct authored_world_bounds_t
    {
        int32_t x{ 0 };
        int32_t y{ 0 };
        int32_t width{ 0 };
        int32_t height{ 0 };

        [[nodiscard]] bool overlaps(const authored_world_bounds_t& other) const noexcept;
        [[nodiscard]] bool contains(int32_t px, int32_t py) const noexcept;
    };

    struct runtime_world_chunk_identity_t
    {
        std::string world_source_uri;
        std::string map_source_uri;
        uint32_t authored_chunk_index{ 0u };

        [[nodiscard]] std::string stable_id() const;
    };

    struct runtime_world_chunk_descriptor_t
    {
        runtime_world_chunk_identity_t identity;
        std::string map_file_name;
        authored_world_bounds_t authored_bounds_px;
    };

    enum class runtime_world_chunk_residency_t : uint8_t
    {
        unloaded = 0,
        resident,
        active
    };

    struct runtime_world_chunk_state_t
    {
        runtime_world_chunk_descriptor_t descriptor;
        runtime_world_chunk_residency_t residency{ runtime_world_chunk_residency_t::unloaded };
        bool tilemap_visuals_imported{ false };
        bool collision_imported{ false };
        bool objects_imported{ false };
    };

    class runtime_world_layout_t
    {
    public:
        [[nodiscard]] static runtime_world_layout_t from_authored_world(const assets::tiled_world_t& world);

        [[nodiscard]] std::span<const runtime_world_chunk_descriptor_t> chunks() const noexcept { return _chunks; }
        [[nodiscard]] const runtime_world_chunk_descriptor_t* find_by_map_source_uri(std::string_view map_source_uri) const noexcept;
        [[nodiscard]] std::vector<const runtime_world_chunk_descriptor_t*> query_overlapping(
            const authored_world_bounds_t& bounds) const;
        [[nodiscard]] std::vector<runtime_world_chunk_state_t> instantiate_chunk_states() const;

    private:
        std::vector<runtime_world_chunk_descriptor_t> _chunks;
    };
} // namespace carrot::world
