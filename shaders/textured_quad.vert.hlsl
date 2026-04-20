#include "ForwardPlusData.hlsli"
#include "Renderer/Draw/WorldRenderItemShared.h"

CARROT_VK_BINDING(0, 0)
cbuffer WorldForwardPlus
{
    float4x4 g_view_projection;
    float4 g_ambient_color;
    uint4 g_renderer_flags;
    ForwardPlusFrameConstants g_forward_plus;
    PointLightData g_point_lights[CARROT_MAX_WORLD_POINT_LIGHTS];
    ForwardPlusTileHeader g_forward_plus_tiles[CARROT_MAX_FORWARD_PLUS_TILES];
    uint4 g_forward_plus_light_indices[CARROT_MAX_FORWARD_PLUS_PACKED_LIGHT_INDEX_WORDS];
};

CARROT_DECLARE_BYTE_ADDRESS_BUFFER(g_world_item_buffer, 3, 0, 2);
CARROT_DECLARE_BYTE_ADDRESS_BUFFER(g_visible_item_index_buffer, 4, 0, 3);

struct VSInput
{
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float effect_mode : TEXCOORD1;
    float effect_param0 : TEXCOORD2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float effect_mode : TEXCOORD1;
    float effect_param0 : TEXCOORD2;
    float2 world_position_px : TEXCOORD3;
};

GpuWorldRenderItem load_world_item(uint item_index)
{
    const uint byte_offset = item_index * 80u;

    GpuWorldRenderItem item;
    item.quad_rect_px = asfloat(g_world_item_buffer.Load4(byte_offset));
    item.uv_rect = asfloat(g_world_item_buffer.Load4(byte_offset + 16u));
    item.bounds_min_max_px = asfloat(g_world_item_buffer.Load4(byte_offset + 32u));
    item.packed_data = g_world_item_buffer.Load4(byte_offset + 48u);
    item.draw_params = asfloat(g_world_item_buffer.Load4(byte_offset + 64u));
    return item;
}

float4 unpack_abgr_color(uint packed_color)
{
    return float4(
        (float)((packed_color >> 0u) & 0xFFu) / 255.0f,
        (float)((packed_color >> 8u) & 0xFFu) / 255.0f,
        (float)((packed_color >> 16u) & 0xFFu) / 255.0f,
        (float)((packed_color >> 24u) & 0xFFu) / 255.0f
    );
}

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
VSOutput main(VSInput input, uint instance_id : SV_InstanceID)
{
    VSOutput output;
    float2 position = input.position;
    float2 uv = input.uv;
    float4 color = input.color;
    float effect_mode = input.effect_mode;
    float effect_param0 = input.effect_param0;

    if (g_renderer_flags.x != 0u)
    {
        const uint visible_item_index = g_visible_item_index_buffer.Load(instance_id * 4u);
        const GpuWorldRenderItem item = load_world_item(visible_item_index);

        position = item.quad_rect_px.xy + (input.position * item.quad_rect_px.zw);
        uv = lerp(item.uv_rect.xy, item.uv_rect.zw, input.uv);
        color = unpack_abgr_color(item.packed_data.x);
        effect_mode = item.draw_params.x;
        effect_param0 = item.draw_params.y;
    }

    float4 clip = mul(g_view_projection, float4(position, 0.0f, 1.0f));
    clip.y *= CARROT_CLIP_SPACE_Y_SIGN;

    output.position = clip;
    output.uv = uv;
    output.color = color;
    output.effect_mode = effect_mode;
    output.effect_param0 = effect_param0;
    output.world_position_px = position;

    return output;
}
