#include "ShaderCommon.h"

CARROT_VK_BINDING(0, 0)
cbuffer TexturedQuadCamera
{
    float4x4 g_view_projection;
};

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
};

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
VSOutput main(VSInput input)
{
    VSOutput output;

    float4 clip = mul(g_view_projection, float4(input.position, 0.0f, 1.0f));
    clip.y *= CARROT_CLIP_SPACE_Y_SIGN;

    output.position = clip;
    output.uv = input.uv;
    output.color = input.color;
    output.effect_mode = input.effect_mode;
    output.effect_param0 = input.effect_param0;

    return output;
}
