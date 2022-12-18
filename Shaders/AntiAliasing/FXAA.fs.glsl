#version 460

layout(binding = 0) uniform sampler2D _Texture;

// XXX: Maybe pass in luminance texture directly since it is previously computed?
// layout(binding = 1) uniform sampler2D _TextureLuminance;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(std140, binding = 13) uniform FXAAParams
{
    float _threshold;
    float _relativeThreshold;
};

void main()
{
    vec2 texSize = vec2(textureSize(_Texture, 0));
    vec2 texelSize = 1.0 / texSize;

    // Sample 4 closest pixels.
    vec3 n = textureOffset(_Texture, uv, ivec2(0.0, -1.0)).rgb;
    vec3 s = textureOffset(_Texture, uv, ivec2(0.0, 1.0)).rgb;
    vec3 e = textureOffset(_Texture, uv, ivec2(1.0, 0.0)).rgb;
    vec3 w = textureOffset(_Texture, uv, ivec2(-1.0, 0.0)).rgb;

    vec3 currentColor = texture(_Texture, uv).rgb;

    // Luminance constant.
    const vec3 luminanceConstant = vec3(0.2126, 0.7152, 0.0722);

    float brightnessN = dot(n, luminanceConstant);
    float brightnessS = dot(s, luminanceConstant);
    float brightnessE = dot(e, luminanceConstant);
    float brightnessW = dot(w, luminanceConstant);
    float brightnessCurrent = dot(currentColor, luminanceConstant);

    float brightnessCurrentin
        = min(brightnessCurrent, min(min(brightnessN, brightnessS), min(brightnessE, brightnessW)));
    float brightnessCurrentMax
        = max(brightnessCurrent, max(max(brightnessN, brightnessS), max(brightnessE, brightnessW)));

    float contrast = brightnessCurrentMax - brightnessCurrentin;

    float thr = _threshold;
    float relThr = _relativeThreshold;

    float threshold = max(thr, relThr * brightnessCurrentMax);

    if (contrast < threshold)
    {
        out_FragColor = vec4(currentColor, 1.0);
    }
    else
    {
        vec3 nw = texture(_Texture, uv + (vec2(-1.0, -1.0) * texelSize)).rgb;
        vec3 ne = texture(_Texture, uv + (vec2(1.0, -1.0) * texelSize)).rgb;
        vec3 sw = texture(_Texture, uv + (vec2(-1.0, 1.0) * texelSize)).rgb;
        vec3 se = texture(_Texture, uv + (vec2(1.0, 1.0) * texelSize)).rgb;

        float brightnessNW = dot(nw, luminanceConstant);
        float brightnessNE = dot(ne, luminanceConstant);
        float brightnessSW = dot(sw, luminanceConstant);
        float brightnessSE = dot(se, luminanceConstant);

        float factor = 2 * (brightnessN + brightnessS + brightnessE + brightnessW);
        factor += (brightnessNW + brightnessNE + brightnessSW + brightnessSE);
        factor *= (1.0 / 12.0);
        factor = abs(factor - brightnessCurrent);
        factor = clamp(factor / contrast, 0.0, 1.0);
        factor = smoothstep(0.0, 1.0, factor);
        factor = factor * factor;

        float horizontal = abs(brightnessN + brightnessS - (2 * brightnessCurrent)) * 2
                           + abs(brightnessNE + brightnessSE - (2 * brightnessE))
                           + abs(brightnessNW + brightnessSW - (2 * brightnessW));
        float vertical = abs(brightnessE + brightnessW - (2 * brightnessCurrent)) * 2
                         + abs(brightnessNE + brightnessSE - (2 * brightnessN))
                         + abs(brightnessNW + brightnessSW - (2 * brightnessS));

        bool isHorizontal = horizontal > vertical;

        float pixelStep = isHorizontal ? texelSize.y : texelSize.x;

        float posBrightness = isHorizontal ? brightnessS : brightnessE;
        float negBrightness = isHorizontal ? brightnessN : brightnessW;
        float posGradient = abs(posBrightness - brightnessCurrent);
        float negGradient = abs(negBrightness - brightnessCurrent);

        pixelStep *= (posGradient < negGradient) ? -1 : 1;

        vec2 blendUV = uv;

        // Determine blend direction.
        if (isHorizontal)
        {
            blendUV.y = uv.y + (pixelStep * factor);
        }
        else
        {
            blendUV.x = uv.x + (pixelStep * factor);
        }

        out_FragColor = texture(_Texture, blendUV);
    }
}
