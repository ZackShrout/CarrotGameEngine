#version 460
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D fontSampler;

void main()
{
    float a = texture(fontSampler, inUV).r;
    outColor = vec4(1.0, 0.0, 0.0, a);  // red with alpha from font atlas
}