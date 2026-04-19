#include "ForwardPlusData.hlsli"

CARROT_VK_BINDING(0, 0)
cbuffer WorldForwardPlus
{
    float4x4 g_view_projection;
    float4 g_ambient_color;
    ForwardPlusFrameConstants g_forward_plus;
    PointLightData g_point_lights[CARROT_MAX_WORLD_POINT_LIGHTS];
    ForwardPlusTileHeader g_forward_plus_tiles[CARROT_MAX_FORWARD_PLUS_TILES];
    uint4 g_forward_plus_light_indices[CARROT_MAX_FORWARD_PLUS_PACKED_LIGHT_INDEX_WORDS];
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
    float2 world_position_px : TEXCOORD3;
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
    output.world_position_px = input.position;

    return output;
}
