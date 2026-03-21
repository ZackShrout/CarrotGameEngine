Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 main(PSInput input) : SV_Target
{
    float4 texel = g_texture.Sample(g_sampler, input.uv);
    return texel * input.color;
}
