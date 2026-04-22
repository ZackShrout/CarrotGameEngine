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

CARROT_DECLARE_TEXTURE_2D(g_texture, 0, 1, 4);
CARROT_DECLARE_SAMPLER_STATE(g_sampler, 1, 1, 0);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float effect_mode : TEXCOORD1;
    float effect_param0 : TEXCOORD2;
    float2 world_position_px : TEXCOORD3;
};

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
float4 main(PSInput input) : SV_Target
{
    const float3 bloom = g_texture.Sample(g_sampler, input.uv).rgb;
    const float intensity = max(input.effect_param0, 1.0f);
    return float4(bloom * input.color.rgb * intensity, 1.0f);
}
