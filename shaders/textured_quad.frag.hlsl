#include "ShaderCommon.h"

struct PointLightData
{
    float4 position_radius;
    float4 color_intensity;
};

struct ForwardPlusTileHeader
{
    uint4 data;
};

CARROT_VK_BINDING(0, 0)
cbuffer WorldForwardPlus
{
    float4x4 g_view_projection;
    float4 g_ambient_color;
    float4 g_forward_plus_grid_params;
    uint4 g_forward_plus_tile_counts;
    uint4 g_point_light_counts;
    PointLightData g_point_lights[CARROT_MAX_WORLD_POINT_LIGHTS];
    ForwardPlusTileHeader g_forward_plus_tiles[CARROT_MAX_FORWARD_PLUS_TILES];
    uint4 g_forward_plus_light_indices[CARROT_MAX_FORWARD_PLUS_PACKED_LIGHT_INDEX_WORDS];
};

uint unpack_forward_plus_light_index(uint packed_index)
{
    const uint4 packed = g_forward_plus_light_indices[packed_index / 4];
    switch (packed_index % 4)
    {
        case 0: return packed.x;
        case 1: return packed.y;
        case 2: return packed.z;
        default: return packed.w;
    }
}

CARROT_VK_BINDING(0, 1)
Texture2D g_texture;

CARROT_VK_BINDING(1, 1)
SamplerState g_sampler;

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float effect_mode : TEXCOORD1;
    float effect_param0 : TEXCOORD2;
    float2 world_position_px : TEXCOORD3;
};

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
float4 main(PSInput input) : SV_Target
{
    float3 lighting = g_ambient_color.rgb;
    const float tile_size = max(g_forward_plus_grid_params.z, 1.0f);
    const float2 local_world_position_px = input.world_position_px - g_forward_plus_grid_params.xy;

    if (g_forward_plus_tile_counts.x > 0 && g_forward_plus_tile_counts.y > 0 &&
        local_world_position_px.x >= 0.0f && local_world_position_px.y >= 0.0f)
    {
        const uint tile_x = (uint)(local_world_position_px.x / tile_size);
        const uint tile_y = (uint)(local_world_position_px.y / tile_size);

        if (tile_x < g_forward_plus_tile_counts.x && tile_y < g_forward_plus_tile_counts.y)
        {
            const uint tile_index = tile_y * g_forward_plus_tile_counts.x + tile_x;
            const uint tile_light_offset = g_forward_plus_tiles[tile_index].data.x;
            const uint tile_light_count = g_forward_plus_tiles[tile_index].data.y;

            [loop]
            for (uint i = 0; i < tile_light_count; ++i)
            {
                const uint light_index = unpack_forward_plus_light_index(tile_light_offset + i);
                const float2 light_delta = g_point_lights[light_index].position_radius.xy - input.world_position_px;
                const float light_radius = max(g_point_lights[light_index].position_radius.z, 0.0001f);
                const float normalized_distance = saturate(length(light_delta) / light_radius);
                const float attenuation = 1.0f - (normalized_distance * normalized_distance);
                lighting += g_point_lights[light_index].color_intensity.rgb *
                            (g_point_lights[light_index].color_intensity.a * attenuation);
            }
        }
    }

    return g_texture.Sample(g_sampler, input.uv) * input.color * float4(lighting, g_ambient_color.a);
}
