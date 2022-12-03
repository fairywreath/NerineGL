// https://github.com/ZaOniRinku/NeigeEngine/blob/main/src/graphics/effects/fxaa/FXAA.cpp
#version 460

layout(binding = 0) uniform sampler2D _Texture;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(std140, location = 15) uniform FXAAParams
{
    float _threshold;
    float _relativeThreshold;
};

const float RELATIVE_THRESHOLD = 0.125;
const float THRESHOLD = 0.0312;

void main()
{
    vec2 texSize = vec2(textureSize(_Texture, 0));
    vec2 texelSize = 1.0 / texSize;

    // Sample 4 closest.
    vec3 n = texture(_Texture, uv + (vec2(0.0, -1.0) * texelSize)).rgb;
    vec3 s = texture(_Texture, uv + (vec2(0.0, 1.0) * texelSize)).rgb;
    vec3 e = texture(_Texture, uv + (vec2(1.0, 0.0) * texelSize)).rgb;
    vec3 w = texture(_Texture, uv + (vec2(-1.0, 0.0) * texelSize)).rgb;
    vec3 m = texture(_Texture, uv).rgb;

    // Luminance constant.
    vec3 luminanceConstant = vec3(0.2127, 0.7152, 0.0722);

    float brightnessN = dot(n, luminanceConstant);
    float brightnessS = dot(s, luminanceConstant);
    float brightnessE = dot(e, luminanceConstant);
    float brightnessW = dot(w, luminanceConstant);
    float brightnessM = dot(m, luminanceConstant);

    float brightnessMin
        = min(brightnessM, min(min(brightnessN, brightnessS), min(brightnessE, brightnessW)));
    float brightnessMax
        = max(brightnessM, max(max(brightnessN, brightnessS), max(brightnessE, brightnessW)));

    float contrast = brightnessMax - brightnessMin;

    float threshold = max(THRESHOLD, RELATIVE_THRESHOLD * brightnessMax);

    if (contrast < threshold)
    {
        out_FragColor = vec4(m, 1.0);
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
        factor = abs(factor - brightnessM);
        factor = clamp(factor / contrast, 0.0, 1.0);
        factor = smoothstep(0.0, 1.0, factor);
        factor = factor * factor;

        float horizontal = abs(brightnessN + brightnessS - (2 * brightnessM)) * 2
                           + abs(brightnessNE + brightnessSE - (2 * brightnessE))
                           + abs(brightnessNW + brightnessSW - (2 * brightnessW));
        float vertical = abs(brightnessE + brightnessW - (2 * brightnessM)) * 2
                         + abs(brightnessNE + brightnessSE - (2 * brightnessN))
                         + abs(brightnessNW + brightnessSW - (2 * brightnessS));

        bool isHorizontal = horizontal > vertical;

        float pixelStep = isHorizontal ? texelSize.y : texelSize.x;

        float posBrightness = isHorizontal ? brightnessS : brightnessE;
        float negBrightness = isHorizontal ? brightnessN : brightnessW;
        float posGradient = abs(posBrightness - brightnessM);
        float negGradient = abs(negBrightness - brightnessM);

        pixelStep *= (posGradient < negGradient) ? -1 : 1;

        vec2 blendUV = uv;
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
