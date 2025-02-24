#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstant
{
    bool ibHorizontal;
};

layout(set = 0, binding = 0) uniform GameArgs
{
    vec2 iResolution;
};

layout(set = 1, binding = 0) uniform sampler2D iBloomTex;

vec3 ColorFetch(vec2 TexCoord)
{
    if (TexCoord.x < 0.00001 || TexCoord.x > 0.99999 || TexCoord.y < 0.00001 || TexCoord.y > 0.99999)
    {
        return vec3(0.0);
    }
    return texture(iBloomTex, TexCoord).rgb;
}

vec3 Grab1(vec2 TexCoord, float Octave, vec2 Offset)
{
    float Scale = exp2(Octave);

    TexCoord += Offset;
    TexCoord *= Scale;

    if (TexCoord.x < 0.0 || TexCoord.x > 1.0 || TexCoord.y < 0.0 || TexCoord.y > 1.0)
    {
        return vec3(0.0);
    }

    vec3 Color = ColorFetch(TexCoord);
    return Color;
}

vec3 GrabN(vec2 TexCoord, float Octave, vec2 Offset, int OverSamples)
{
    float Scale = exp2(Octave);

    TexCoord += Offset;
    TexCoord *= Scale;

    if (TexCoord.x < 0.0 || TexCoord.x > 1.0 || TexCoord.y < 0.0 || TexCoord.y > 1.0)
    {
        return vec3(0.0);
    }

    vec3  Color   = vec3(0.0);
    float Weights = 0.0;

    for (int i = 0; i < OverSamples; ++i)
    {
        for (int j = 0; j < OverSamples; ++j)
        {
            vec2 Offset = (vec2(i, j) / iResolution.xy + vec2(-float(OverSamples) * 0.5) / iResolution.xy) * Scale / float(OverSamples);
            Color += ColorFetch(TexCoord + Offset);

            Weights += 1.0;
        }
    }

    Color /= Weights;
    return Color;
}

vec2 CalcOffset(float Octave)
{
    vec2 Offset  = vec2(0.0);
    vec2 Padding = vec2(10.0) / iResolution;

    Offset.x = -min(1.0, floor(Octave / 3.0)) * (0.25 + Padding.x);
    Offset.y = -(1.0 - (1.0 / exp2(Octave))) - Padding.y * Octave;
    Offset.y += min(1.0, floor(Octave / 3.0)) * 0.35;

    return Offset;
}

vec3 GetMipmapTree(vec2 FragUv)
{
    vec3 Color = vec3(0.0);

    Color += Grab1(FragUv, 1.0, vec2(0.0, 0.0));
    Color += GrabN(FragUv, 2.0, vec2(CalcOffset(1.0)), 4);
    Color += GrabN(FragUv, 3.0, vec2(CalcOffset(2.0)), 8);
    Color += GrabN(FragUv, 4.0, vec2(CalcOffset(3.0)), 16);
    Color += GrabN(FragUv, 5.0, vec2(CalcOffset(4.0)), 16);
    Color += GrabN(FragUv, 6.0, vec2(CalcOffset(5.0)), 16);
    Color += GrabN(FragUv, 7.0, vec2(CalcOffset(6.0)), 16);
    Color += GrabN(FragUv, 8.0, vec2(CalcOffset(7.0)), 16);

    return Color;
   // return vec3(0.,.,2.);
}

vec3 GaussBlur(vec2 FragUv, bool bHorizontal)
{
    float Weights[5] = { 0.19638062, 0.29675293, 0.09442139, 0.01037598, 0.00025940 };
    float Offsets[5] = { 0.00000000, 1.41176471, 3.29411765, 5.17647059, 7.05882353 };

    vec3  Color     = vec3(0.0);
    float WeightSum = 0.0;

    vec2 Step = bHorizontal ? vec2(0.5, 0.0) : vec2(0.0, 0.5);
    
    if (FragUv.x < 0.52)
    {
        Color += ColorFetch(FragUv) * Weights[0];
        WeightSum += Weights[0];

        for(int i = 1; i < 5; ++i)
        {
            vec2 Offset = vec2(Offsets[i]) / iResolution.xy;
            Color += ColorFetch(FragUv + Offset * Step) * Weights[i];
            Color += ColorFetch(FragUv - Offset * Step) * Weights[i];
            WeightSum += Weights[i] * 2.0;
        }

        Color /= WeightSum;
    }

    return Color;
    //return ColorFetch(FragUv);
}

void main()
{
    vec2 FragUv = gl_FragCoord.xy / iResolution;
#if defined(GENERATE_MIPMAP)
    FragColor = vec4(GetMipmapTree(FragUv), 1.0);
#elif defined(GAUSS_BLUR)
    FragColor = vec4(GaussBlur(FragUv, ibHorizontal), 1.0);
#endif
}