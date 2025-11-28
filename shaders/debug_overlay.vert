#version 460
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec2 outUV;

layout(push_constant) uniform Push {
    vec2 u_Resolution;
} push;

void main()
{
    vec2 pos = inPos;
    pos.y = push.u_Resolution.y - pos.y;  // convert to top-left origin
    pos /= push.u_Resolution;
    pos = pos * 2.0 - 1.0;
    pos.y = -pos.y;  // ← THIS IS THE MISSING LINE — Vulkan Y is down!

    gl_Position = vec4(pos, 0.0, 1.0);
    outUV = inUV;
}