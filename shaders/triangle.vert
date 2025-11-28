#version 460

layout(push_constant) uniform PushConstants {
    uint frameCount;
} pc;

layout(location = 0) out vec3 color;

const vec2 positions[3] = vec2[](
    vec2( 0.0, -0.8),
    vec2(-0.7,  0.7),
    vec2( 0.7,  0.7)
);

const vec3 colors[3] = vec3[](
    vec3(1.0, 0.5, 0.0), // carrot orange
    vec3(1.0, 0.6, 0.2),
    vec3(1.0, 0.7, 0.3)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    color = colors[gl_VertexIndex];

    // SPINNING — using push constant
    float angle = float(pc.frameCount) * 0.02 + float(gl_VertexIndex) * 1.0;
    mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    gl_Position.xy = rot * gl_Position.xy;
}
