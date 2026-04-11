#include "ShaderCommon.h"

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
};

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
float4 main(PSInput input) : SV_Target
{
    return g_texture.Sample(g_sampler, input.uv) * input.color;
}
