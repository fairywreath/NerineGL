#version 460 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureCurrent;
layout(binding = 1) uniform sampler2D _TextureHistory;

// constexpr sampler sam_point(min_filter::nearest, mag_filter::nearest, mip_filter::none);
// constexpr sampler sam_linear(min_filter::linear, mag_filter::linear, mip_filter::none);

void main()
{
    vec4 colorCurrent = texture(_TextureCurrent, uv);
    vec4 colorHistory = texture(_TextureHistory, uv);

    float modulationFactor = 0.9;

    // vec4 colorMixed = colorCurrent + ((colorHistory - colorCurrent) * modulationFactor);

    // out_FragColor = vec4(colorMixed.rgb, 1.0);

    // out_FragColor = vec4((colorCurrent - colorHistory).rgb, 1.0);
    // out_FragColor = vec4((colorCurrent - colorHistory).rgb * 100, 1.0);

    out_FragColor = vec4(mix(colorCurrent, colorHistory, modulationFactor).rgb, 1.0);

    // out_FragColor = vec4(colorCurrent.rgb, 1.0);
    // out_FragColor = vec4(colorHistory.rgb, 1.0);
}
