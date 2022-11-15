#version 460 core

layout(location = 0) out vec2 uv;

vec4 TrianglePosition(int vtx)
{
    float x = -1.0 + float((vtx & 1) << 2);
    float y = -1.0 + float((vtx & 2) << 1);
    return vec4(x, y, 0.0, 1.0);
}
vec2 TriangleUV(int vtx)
{
    float u = (vtx == 1) ? 2.0 : 0.0;
    float v = (vtx == 2) ? 2.0 : 0.0;
    return vec2(u, v);
}

void main()
{
    gl_Position = TrianglePosition(int(gl_VertexID));
    uv = TriangleUV(int(gl_VertexID));
}
