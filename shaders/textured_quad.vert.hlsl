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

struct QuadPushConstants
{
    float2 offset;
    float2 scale;
    float2 uv_min;
    float2 uv_max;
    float4 tint;
};

#if defined(VULKAN)

[[vk::push_constant]]
QuadPushConstants g_push;

#else // METAL + DX12

ConstantBuffer<QuadPushConstants> g_push : register(b0);

#endif

VSOutput main(VSInput input)
{
    VSOutput output;

    float2 pos = input.position * g_push.scale + g_push.offset;
    output.position = float4(pos, 0.0f, 1.0f);

    output.uv = lerp(g_push.uv_min, g_push.uv_max, input.uv);
    output.color = input.color * g_push.tint;

    return output;
}
