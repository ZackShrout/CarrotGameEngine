#include "ShaderCommon.h"

CARROT_DECLARE_TEXTURE_2D(g_texture, 0, 1, 2);
CARROT_DECLARE_SAMPLER_STATE(g_sampler, 1, 1, 0);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float effect_param0 : TEXCOORD2;
};

float median3(float a, float b, float c)
{
    return max(min(a, b), min(max(a, b), c));
}

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
float4 main(PSInput input) : SV_Target
{
    float4 texel = g_texture.Sample(g_sampler, input.uv);

    uint width = 0;
    uint height = 0;
    g_texture.GetDimensions(width, height);

    float2 texture_size = max(float2((float)width, (float)height), float2(1.0f, 1.0f));
    float distance_sample = median3(texel.r, texel.g, texel.b);
    float2 unit_range = float2(input.effect_param0, input.effect_param0) / texture_size;
    float2 uv_fwidth = max(fwidth(input.uv), float2(1.0e-6f, 1.0e-6f));
    float2 screen_tex_size = 1.0f / uv_fwidth;
    float screen_px_range = max(min(unit_range.x * screen_tex_size.x,
                                    unit_range.y * screen_tex_size.y), 1.0f);
    float edge_width = 0.5f / screen_px_range;
    float opacity = smoothstep(0.5f - edge_width, 0.5f + edge_width, distance_sample);
    return float4(input.color.rgb, input.color.a * opacity);
}
