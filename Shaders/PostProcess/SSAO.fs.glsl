#version 460 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureDepth;
layout(binding = 1) uniform sampler2D _TextureRotation;

layout(std140, binding = 0) uniform SSAOParams
{
    float scale;
    float bias;
    float zNear;
    float zFar;
    float radius;
    float attScale;
    float distScale;
};

const vec3 offsets[8] = vec3[8](vec3(-0.5, -0.5, -0.5), vec3(0.5, -0.5, -0.5),
                                vec3(-0.5, 0.5, -0.5), vec3(0.5, 0.5, -0.5), vec3(-0.5, -0.5, 0.5),
                                vec3(0.5, -0.5, 0.5), vec3(-0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5));

void main()
{
    float size = 1.0 / float(textureSize(_TextureDepth, 0).x);

    float Z = (zFar * zNear) / (texture(_TextureDepth, uv).x * (zFar - zNear) - zFar);
    float att = 0.0;
    vec3 plane = texture(_TextureRotation, uv * size / 4.0).xyz - vec3(1.0);

    for (int i = 0; i < 8; i++)
    {
        vec3 rSample = reflect(offsets[i], plane);
        float zSample = texture(_TextureDepth, uv + radius * rSample.xy / Z).x;

        zSample = (zFar * zNear) / (zSample * (zFar - zNear) - zFar);

        float dist = max(zSample - Z, 0.0) / distScale;
        float occl = 15.0 * max(dist * (2.0 - dist), 0.0);

        att += 1.0 / (1.0 + occl * occl);
    }

    att = clamp(att * att / 64.0 + 0.45, 0.0, 1.0) * attScale;
    out_FragColor = vec4(vec3(att), 1.0);
}
