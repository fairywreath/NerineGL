layout(std140, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 light;
    vec4 cameraPos;
    vec4 frustumPlanes[6];
    vec4 frustumCorners[8];
    uint numShapesToCull;

    vec2 taaJitterOffset;
};

layout(std140, binding = 10) uniform FrameData
{
    mat4 prevView;
    mat4 prevProj;

    // ivec2 currViewSize;
    ivec2 resolution2;
    // vec2 resolution;
};
