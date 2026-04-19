#include "ForwardPlusData.hlsli"

struct ComputeForwardPlusConstants
{
    float4 grid_params;
    uint4 tile_counts;
    uint4 point_light_counts;
};

CARROT_DECLARE_RWBYTE_ADDRESS_BUFFER(g_forward_plus_constants_buffer, 0, 0, 0);
CARROT_DECLARE_RWBYTE_ADDRESS_BUFFER(g_forward_plus_light_input_buffer, 1, 0, 1);
CARROT_DECLARE_RWBYTE_ADDRESS_BUFFER(g_forward_plus_output_buffer, 2, 0, 2);
CARROT_DECLARE_RWBYTE_ADDRESS_BUFFER(g_unused_storage3, 3, 0, 3);

float4 load_grid_params()
{
    return asfloat(g_forward_plus_constants_buffer.Load4(0u));
}

uint4 load_tile_counts()
{
    return g_forward_plus_constants_buffer.Load4(16u);
}

uint4 load_point_light_counts()
{
    return g_forward_plus_constants_buffer.Load4(32u);
}

PointLightData load_point_light(uint light_index)
{
    const uint byte_offset = light_index * 32u;

    PointLightData light;
    light.position_radius = asfloat(g_forward_plus_light_input_buffer.Load4(byte_offset));
    light.color_intensity = asfloat(g_forward_plus_light_input_buffer.Load4(byte_offset + 16u));
    return light;
}

void store_tile_header(uint tile_index, uint light_index_offset, uint light_count)
{
    const uint byte_offset = tile_index * 16u;
    g_forward_plus_output_buffer.Store4(byte_offset, uint4(light_index_offset, light_count, 0u, 0u));
}

void store_light_index(uint packed_index, uint light_index)
{
    const uint packed_light_indices_base = CARROT_MAX_FORWARD_PLUS_TILES * 16u;
    g_forward_plus_output_buffer.Store(packed_light_indices_base + packed_index * 4u, light_index);
}

bool circle_overlaps_aabb(float2 center, float radius, float2 aabb_min, float2 aabb_max)
{
    const float2 clamped = clamp(center, aabb_min, aabb_max);
    const float2 delta = center - clamped;
    return dot(delta, delta) <= (radius * radius);
}

CARROT_ROOT_SIGNATURE(CARROT_RS_COMPUTE)
[numthreads(64, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint4 tile_counts = load_tile_counts();
    const uint tile_count_x = tile_counts.x;
    const uint tile_count_y = tile_counts.y;
    const uint tile_count = tile_count_x * tile_count_y;
    if (dispatch_thread_id.x >= tile_count)
        return;

    const uint tile_index = dispatch_thread_id.x;
    const uint tile_x = tile_index % tile_count_x;
    const uint tile_y = tile_index / tile_count_x;

    const float4 grid_params = load_grid_params();
    const float tile_size = max(grid_params.z, 1.0f);
    const float2 tile_min = float2(grid_params.x + tile_x * tile_size, grid_params.y + tile_y * tile_size);
    const float2 tile_max = tile_min + float2(tile_size, tile_size);

    const uint light_index_offset = tile_index * CARROT_MAX_WORLD_POINT_LIGHTS;
    uint light_count = 0u;

    const uint point_light_count = min(load_point_light_counts().x, CARROT_MAX_WORLD_POINT_LIGHTS);
    [loop]
    for (uint light_index = 0u; light_index < point_light_count; ++light_index)
    {
        const PointLightData light = load_point_light(light_index);
        if (!circle_overlaps_aabb(light.position_radius.xy, light.position_radius.z, tile_min, tile_max))
            continue;

        store_light_index(light_index_offset + light_count, light_index);
        ++light_count;
    }

    store_tile_header(tile_index, light_index_offset, light_count);
}
