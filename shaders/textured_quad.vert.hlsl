#include "ShaderCommon.h"

struct VSInput
{
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
VSOutput main(VSInput input)
{
    VSOutput output;

    const float2 clip_pos = CARROT_APPLY_CLIP_SPACE_Y(input.position);

    output.position = float4(clip_pos, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = input.color;

    return output;
}
