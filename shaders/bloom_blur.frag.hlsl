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
    const float texel_step = max(abs(input.effect_param0), 0.00001f);
    const bool horizontal = input.effect_mode >= 0.0f;
    const float2 axis = horizontal ? float2(texel_step, 0.0f) : float2(0.0f, texel_step);

    // Standard-ish separable Gaussian bloom weights. Wider and softer than the earlier boxier blur.
    float4 color =
        g_texture.Sample(g_sampler, input.uv - axis * 4.0f) * 0.01621622f +
        g_texture.Sample(g_sampler, input.uv - axis * 3.0f) * 0.05405405f +
        g_texture.Sample(g_sampler, input.uv - axis * 2.0f) * 0.12162162f +
        g_texture.Sample(g_sampler, input.uv - axis) * 0.19459459f +
        g_texture.Sample(g_sampler, input.uv) * 0.22702703f +
        g_texture.Sample(g_sampler, input.uv + axis) * 0.19459459f +
        g_texture.Sample(g_sampler, input.uv + axis * 2.0f) * 0.12162162f +
        g_texture.Sample(g_sampler, input.uv + axis * 3.0f) * 0.05405405f +
        g_texture.Sample(g_sampler, input.uv + axis * 4.0f) * 0.01621622f;

    return color * input.color;
}
