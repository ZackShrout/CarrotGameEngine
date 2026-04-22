#include "ForwardPlusData.hlsli"

CARROT_DECLARE_TEXTURE_2D(g_texture, 0, 1, 4);
CARROT_DECLARE_SAMPLER_STATE(g_sampler, 1, 1, 0);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float swirl_direction : TEXCOORD1;
    float progress : TEXCOORD2;
    float2 world_position_px : TEXCOORD3;
};

CARROT_ROOT_SIGNATURE(CARROT_RS_TEXTURED_QUAD)
float4 main(PSInput input) : SV_Target
{
    const float progress = saturate(input.progress);
    const float2 center = float2(0.5f, 0.5f);
    const float2 centered_uv = input.uv - center;
    const float radius = length(centered_uv);
    const float angle = atan2(centered_uv.y, centered_uv.x);
    const float swirl_strength = (0.8f + 7.2f * progress * progress) * saturate(1.35f - radius);
    const float chaos = 0.06f * progress * progress * sin((radius * 34.0f) - (angle * 5.0f));
    const float radial_scale = max(0.04f, 1.0f - 0.82f * progress + chaos);
    const float warped_angle = angle + (input.swirl_direction * swirl_strength);
    const float2 warped_dir = float2(cos(warped_angle), sin(warped_angle));
    const float2 warped_uv = center + (warped_dir * radius * radial_scale);
    const float2 tangent = float2(-warped_dir.y, warped_dir.x);
    const float smear = 0.02f * progress * progress;
    const float2 sample_a = saturate(warped_uv - tangent * smear);
    const float2 sample_b = saturate(warped_uv);
    const float2 sample_c = saturate(warped_uv + tangent * smear);

    const float4 texel_a = g_texture.Sample(g_sampler, sample_a);
    const float4 texel_b = g_texture.Sample(g_sampler, sample_b);
    const float4 texel_c = g_texture.Sample(g_sampler, sample_c);
    const float4 swirl_color = ((texel_a * 0.25f) + (texel_b * 0.5f) + (texel_c * 0.25f)) * input.color;
    return float4(swirl_color.rgb, 1.0f);
}
