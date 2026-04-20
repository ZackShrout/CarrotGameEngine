#include "ForwardPlusData.hlsli"

CARROT_VK_BINDING(0, 0)
cbuffer WorldForwardPlus
{
    float4x4 g_view_projection;
    float4 g_ambient_color;
    uint4 g_renderer_flags;
    ForwardPlusFrameConstants g_forward_plus;
};

CARROT_DECLARE_BYTE_ADDRESS_BUFFER(g_forward_plus_light_input_buffer, 1, 0, 0);
CARROT_DECLARE_BYTE_ADDRESS_BUFFER(g_forward_plus_output_buffer, 2, 0, 1);

PointLightData load_point_light(uint light_index)
{
    const uint byte_offset = light_index * 32u;

    PointLightData light;
    light.position_radius = asfloat(g_forward_plus_light_input_buffer.Load4(byte_offset));
    light.color_intensity = asfloat(g_forward_plus_light_input_buffer.Load4(byte_offset + 16u));
    return light;
}

CARROT_DECLARE_TEXTURE_2D(g_texture, 0, 1, 4);
CARROT_DECLARE_SAMPLER_STATE(g_sampler, 1, 1, 0);

ForwardPlusTileHeader load_tile_header(uint tile_index)
{
    ForwardPlusTileHeader header;
    header.data = g_forward_plus_output_buffer.Load4(tile_index * 16u);
    return header;
}

uint unpack_forward_plus_light_index(uint packed_index)
{
    const uint packed_light_indices_base = CARROT_MAX_FORWARD_PLUS_TILES * 16u;
    return g_forward_plus_output_buffer.Load(packed_light_indices_base + packed_index * 4u);
}

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
    const float4 base_color = g_texture.Sample(g_sampler, input.uv) * input.color;
    if (base_color.a <= 0.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 lighting = g_ambient_color.rgb;
    const float tile_size = max(g_forward_plus.grid_params.z, 1.0f);
    const float2 local_world_position_px = input.world_position_px - g_forward_plus.grid_params.xy;

    if (g_forward_plus.tile_counts.x > 0 && g_forward_plus.tile_counts.y > 0 &&
        local_world_position_px.x >= 0.0f && local_world_position_px.y >= 0.0f)
    {
        const uint tile_x = (uint)(local_world_position_px.x / tile_size);
        const uint tile_y = (uint)(local_world_position_px.y / tile_size);

        if (tile_x < g_forward_plus.tile_counts.x && tile_y < g_forward_plus.tile_counts.y)
        {
            const uint tile_index = tile_y * g_forward_plus.tile_counts.x + tile_x;
            const ForwardPlusTileHeader tile_header = load_tile_header(tile_index);
            const uint tile_light_offset = tile_header.data.x;
            const uint tile_light_count = tile_header.data.y;

            [loop]
            for (uint i = 0; i < tile_light_count; ++i)
            {
                const uint light_index = unpack_forward_plus_light_index(tile_light_offset + i);
                const PointLightData light = load_point_light(light_index);
                const float2 light_delta = light.position_radius.xy - input.world_position_px;
                const float light_radius = max(light.position_radius.z, 0.0001f);
                const float normalized_distance = saturate(length(light_delta) / light_radius);
                const float attenuation = 1.0f - (normalized_distance * normalized_distance);
                lighting += light.color_intensity.rgb * (light.color_intensity.a * attenuation);
            }
        }
    }

    return base_color * float4(lighting, g_ambient_color.a);
}
