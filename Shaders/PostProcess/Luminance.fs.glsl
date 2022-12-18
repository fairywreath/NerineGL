#version 460 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureScene;

void main()
{
    vec4 color = texture(_TextureScene, uv);

    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

    out_FragColor = vec4(vec3(luminance), 1.0);
}
