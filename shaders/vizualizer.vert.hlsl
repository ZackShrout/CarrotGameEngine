#define MY_ROOT_SIG "RootFlags(0), " \
"CBV(b0, visibility=SHADER_VISIBILITY_ALL)"  // DX12 only; ignored elsewhere

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

struct VisualizerParams
{
    uint  frameCount;
    float time;
    float rms;
    float3 bands;
};

#if defined(VULKAN)
[[vk::push_constant]]
VisualizerParams g_vis;
#else
ConstantBuffer<VisualizerParams> g_vis : register(b0);
#endif

// Classic fullscreen triangle in clip space.
// Covers the whole screen without needing a vertex buffer.
static const float2 positions[3] = {
    float2(-1.0, -3.0),
    float2(-1.0,  1.0),
    float2( 3.0,  1.0)
};

[RootSignature(MY_ROOT_SIG)]
VSOutput main(uint vertexIndex : SV_VertexID)
{
    VSOutput o;
    float2 pos = positions[vertexIndex];
    o.position = float4(pos, 0.0, 1.0);

    // Derive UV from clip-space coordinates: map [-1,1] => [0,1]
    float2 uv = pos * 0.5f + 0.5f;
    o.uv = uv;

    return o;
}