#include "ShaderCommon.h"

CARROT_VK_BINDING(0, 0)
cbuffer TextQuadCamera
{
    float4x4 g_view_projection;
};

struct VSInput
{
    float2 position : POSITION;
    float2 unit_uv : TEXCOORD0;
    float4 quad_rect_px : TEXCOORD1;
    float4 uv_rect : TEXCOORD2;
    float4 color : COLOR0;
    float4 draw_params : TEXCOORD3;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float effect_param0 : TEXCOORD2;
};

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
VSOutput main(VSInput input)
{
    VSOutput output;

    const float2 position = input.quad_rect_px.xy + (input.position * input.quad_rect_px.zw);
    const float2 uv = lerp(input.uv_rect.xy, input.uv_rect.zw, input.unit_uv);

    float4 clip = mul(g_view_projection, float4(position, 0.0f, 1.0f));
    clip.y *= CARROT_CLIP_SPACE_Y_SIGN;

    output.position = clip;
    output.uv = uv;
    output.color = input.color;
    output.effect_param0 = input.draw_params.y;

    return output;
}
