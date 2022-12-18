#version 460 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureScene;

void main()
{
    vec4 color = texture(_TextureScene, uv);

    float luminance = dot(color, vec4(0.33, 0.34, 0.33, 0.0));

    out_FragColor = luminance >= 1.0 ? color : vec4(vec3(0.0), 1.0);
    // out_FragColor = luminance >= 1.2 ? color : vec4(vec3(0.0), 1.0);
}
