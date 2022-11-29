// Simple passthrough fragment shader.
#version 460 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _Texture;

void main()
{
    out_FragColor = texture(_Texture, uv);
}
