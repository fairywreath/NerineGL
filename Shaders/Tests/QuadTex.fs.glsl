#version 460 core

#extension GL_ARB_bindless_texture : require

layout(location = 0) uniform sampler2D _texture;
layout(location = 1) uniform uvec2 _bindlessHandle;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

void main()
{
//	out_FragColor = texture(_texture, vec2(uv.x, -uv.y));
	out_FragColor = texture(sampler2D(_bindlessHandle), uv);

//	out_FragColor = vec4(_bindlessHandle.x / 5000.0, 0, 0, 1);
}
