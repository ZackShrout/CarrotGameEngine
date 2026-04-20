//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#ifdef __cplusplus
#include <array>

#include <chlm/CarrotHLM.h>

namespace carrot::renderer {
    struct gpu_world_render_item_t
    {
        chlm::float4 quad_rect_px{ 0.f, 0.f, 0.f, 0.f };
        chlm::float4 uv_rect{ 0.f, 0.f, 1.f, 1.f };
        chlm::float4 bounds_min_max_px{ 0.f, 0.f, 0.f, 0.f };
        std::array<std::uint32_t, 4> packed_data{ 0u, 0u, 0u, 0u };
        chlm::float4 draw_params{ 0.f, 0.f, 0.f, 0.f };
    };

    struct gpu_world_item_cull_constants_t
    {
        chlm::float4 visible_bounds_px{ 0.f, 0.f, 0.f, 0.f };
        std::array<std::uint32_t, 4> counts{ 0u, 0u, 0u, 0u };
    };

    struct gpu_world_item_cull_state_t
    {
        std::array<std::uint32_t, 4> counts{ 0u, 0u, 0u, 0u };
    };

    struct gpu_world_item_cull_buffer_t
    {
        gpu_world_item_cull_constants_t constants{ };
        gpu_world_item_cull_state_t state{ };
    };
} // namespace carrot::renderer
#else
struct GpuWorldRenderItem
{
    float4 quad_rect_px;
    float4 uv_rect;
    float4 bounds_min_max_px;
    uint4 packed_data;
    float4 draw_params;
};

struct GpuWorldItemCullConstants
{
    float4 visible_bounds_px;
    uint4 counts;
};

struct GpuWorldItemCullState
{
    uint4 counts;
};

struct GpuWorldItemCullBuffer
{
    GpuWorldItemCullConstants constants;
    GpuWorldItemCullState state;
};
#endif
