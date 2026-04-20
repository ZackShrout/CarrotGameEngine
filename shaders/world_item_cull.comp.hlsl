#include "ShaderCommon.h"
#include "Renderer/Draw/WorldRenderItemShared.h"

CARROT_DECLARE_BYTE_ADDRESS_BUFFER(g_world_item_cull_constants_buffer, 0, 0, 0);
CARROT_DECLARE_BYTE_ADDRESS_BUFFER(g_world_item_input_buffer, 1, 0, 1);
CARROT_DECLARE_RWBYTE_ADDRESS_BUFFER(g_visible_item_index_buffer, 2, 0, 2);
CARROT_DECLARE_RWBYTE_ADDRESS_BUFFER(g_world_item_output_buffer, 3, 0, 3);

static const uint k_cull_state_offset = 0u;
static const uint k_indirect_command_offset = 16u;

GpuWorldRenderItem load_world_item(uint item_index)
{
    const uint byte_offset = item_index * 80u;

    GpuWorldRenderItem item;
    item.quad_rect_px = asfloat(g_world_item_input_buffer.Load4(byte_offset));
    item.uv_rect = asfloat(g_world_item_input_buffer.Load4(byte_offset + 16u));
    item.bounds_min_max_px = asfloat(g_world_item_input_buffer.Load4(byte_offset + 32u));
    item.packed_data = g_world_item_input_buffer.Load4(byte_offset + 48u);
    item.draw_params = asfloat(g_world_item_input_buffer.Load4(byte_offset + 64u));
    return item;
}

bool aabb_overlaps(float4 bounds_min_max_px, float4 visible_bounds_px)
{
    return bounds_min_max_px.z > visible_bounds_px.x &&
           bounds_min_max_px.w > visible_bounds_px.y &&
           bounds_min_max_px.x < visible_bounds_px.z &&
           bounds_min_max_px.y < visible_bounds_px.w;
}

GpuWorldItemCullConstants load_cull_constants()
{
    GpuWorldItemCullConstants constants;
    constants.visible_bounds_px = asfloat(g_world_item_cull_constants_buffer.Load4(0u));
    constants.counts = g_world_item_cull_constants_buffer.Load4(16u);
    return constants;
}

CARROT_ROOT_SIGNATURE(CARROT_RS_COMPUTE)
[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x != 0u)
        return;

    const GpuWorldItemCullConstants constants = load_cull_constants();
    const uint item_count = constants.counts.x;
    uint visible_count = 0u;

    [loop]
    for (uint item_index = 0u; item_index < item_count; ++item_index)
    {
        const GpuWorldRenderItem item = load_world_item(item_index);
        if (!aabb_overlaps(item.bounds_min_max_px, constants.visible_bounds_px))
            continue;

        g_visible_item_index_buffer.Store(visible_count * 4u, item_index);
        ++visible_count;
    }

    g_world_item_output_buffer.Store4(k_cull_state_offset, uint4(visible_count, item_count, 0u, 0u));
    g_world_item_output_buffer.Store4(k_indirect_command_offset, uint4(6u, visible_count, 0u, 0u));
    g_world_item_output_buffer.Store(k_indirect_command_offset + 16u, 0u);
}
