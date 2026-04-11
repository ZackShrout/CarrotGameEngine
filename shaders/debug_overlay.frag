#version 460
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D fontSampler;

float median3(float a, float b, float c)
{
    return max(min(a, b), min(max(a, b), c));
}

void main()
{
    vec4 texel = texture(fontSampler, inUV);
    float a = median3(texel.r, texel.g, texel.b);
    outColor = vec4(1.0, 0.0, 0.0, a);  // red with alpha from font atlas
}
