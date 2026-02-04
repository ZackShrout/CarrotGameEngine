#define MY_ROOT_SIG "RootFlags(0), " \
"RootConstants(num32BitConstants=1, b0, visibility=SHADER_VISIBILITY_VERTEX)"

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color    : COLOR0;
};

struct PushConstants
{
    uint frameCount;
};

//[[vk::push_constant]]
//PushConstants pc;               // ← this is the important change

ConstantBuffer<PushConstants> pc : register(b0);

static const float2 positions[3] = {
    float2( 0.0, -0.8),
    float2(-0.7,  0.7),
    float2( 0.7,  0.7)
};

static const float3 colors[3] = {
    float3(1.0, 0.5, 0.0),  // carrot orange
    float3(1.0, 0.6, 0.2),
    float3(1.0, 0.7, 0.3)
};

[RootSignature(MY_ROOT_SIG)]
VSOutput main(uint vertexIndex : SV_VertexID)
{
    VSOutput output;

    float2 pos = positions[vertexIndex];
    output.color = colors[vertexIndex];

    float angle = (float)pc.frameCount * 0.02 + (float)vertexIndex * 1.0;
    float s = cos(angle);
    float c = sin(angle);
    float2x2 rot = float2x2(s, -c, c, s);

    pos = mul(rot, pos);

    output.position = float4(pos, 0.0, 1.0);
    return output;
}