//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

/*
 * Shared renderer/RHI forward+ limits.
 *
 * These values are compiled into renderer code, native backends, and shader
 * compilation. Changing them is parity-sensitive and should be treated as a
 * coordinated renderer-contract change rather than a backend-local tweak.
 */

#define CARROT_MAX_WORLD_POINT_LIGHTS 16
#define CARROT_FORWARD_PLUS_TILE_SIZE_PX 128
#define CARROT_MAX_FORWARD_PLUS_TILES_X 32
#define CARROT_MAX_FORWARD_PLUS_TILES_Y 18
#define CARROT_MAX_FORWARD_PLUS_TILES (CARROT_MAX_FORWARD_PLUS_TILES_X * CARROT_MAX_FORWARD_PLUS_TILES_Y)
#define CARROT_MAX_FORWARD_PLUS_TILE_LIGHT_INDICES (CARROT_MAX_FORWARD_PLUS_TILES * CARROT_MAX_WORLD_POINT_LIGHTS)
#define CARROT_MAX_FORWARD_PLUS_PACKED_LIGHT_INDEX_WORDS ((CARROT_MAX_FORWARD_PLUS_TILE_LIGHT_INDICES + 3) / 4)

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>

namespace carrot::renderer {
    inline constexpr std::size_t k_max_world_point_lights{ static_cast<std::size_t>(CARROT_MAX_WORLD_POINT_LIGHTS) };
    inline constexpr std::uint32_t k_forward_plus_tile_size_px{
        static_cast<std::uint32_t>(CARROT_FORWARD_PLUS_TILE_SIZE_PX)
    };
    inline constexpr std::size_t k_max_forward_plus_tiles_x{
        static_cast<std::size_t>(CARROT_MAX_FORWARD_PLUS_TILES_X)
    };
    inline constexpr std::size_t k_max_forward_plus_tiles_y{
        static_cast<std::size_t>(CARROT_MAX_FORWARD_PLUS_TILES_Y)
    };
    inline constexpr std::size_t k_max_forward_plus_tiles{
        static_cast<std::size_t>(CARROT_MAX_FORWARD_PLUS_TILES)
    };
    inline constexpr std::size_t k_max_forward_plus_tile_light_indices{
        static_cast<std::size_t>(CARROT_MAX_FORWARD_PLUS_TILE_LIGHT_INDICES)
    };
    inline constexpr std::size_t k_max_forward_plus_packed_light_index_words{
        static_cast<std::size_t>(CARROT_MAX_FORWARD_PLUS_PACKED_LIGHT_INDEX_WORDS)
    };
} // namespace carrot::renderer
#endif
