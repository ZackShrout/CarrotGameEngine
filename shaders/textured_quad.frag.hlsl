#include "ShaderCommon.h"

[[vk::binding(0, 1)]]
Texture2D g_texture;

[[vk::binding(1, 1)]]
SamplerState g_sampler;

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
float4 main(PSInput input) : SV_Target
{
    float4 texel = g_texture.Sample(g_sampler, input.uv);
    return texel * input.color;
}