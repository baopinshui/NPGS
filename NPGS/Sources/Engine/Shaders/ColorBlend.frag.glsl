#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform GameArgs
{
    vec2 iResolution;
};

layout(set = 1, binding = 0) uniform sampler2D iBloomTexs[2];

vec3 Saturate(vec3 x)
{
    return clamp(x, vec3(0.0), vec3(1.0));
}

vec4 Cubic(float x)
{
    float x2 = x  * x;
    float x3 = x2 * x;
    vec4  w;
    w.x = -x3 + 3.0 * x2 - 3.0 * x  + 1.0;
    w.y =       3.0 * x3 - 6.0 * x2 + 4.0;
    w.z =      -3.0 * x3 + 3.0 * x2 + 3.0 * x + 1.0;
    w.w =  x3;
    return w / 6.0;
}

vec4 BicubicTexture(in sampler2D Tex, in vec2 TexCoord)
{
    TexCoord *= iResolution;

    float FractX = fract(TexCoord.x);
    float FractY = fract(TexCoord.y);
    TexCoord.x -= FractX;
    TexCoord.y -= FractY;

    FractX -= 0.5;
    FractY -= 0.5;

    vec4 CubicX = Cubic(FractX);
    vec4 CubicY = Cubic(FractY);

    vec4 Coord     = vec4(TexCoord.x - 0.5, TexCoord.x + 1.5, TexCoord.y - 0.5, TexCoord.y + 1.5);
    vec4 WeightSum = vec4(CubicX.x + CubicX.y, CubicX.z + CubicX.w, CubicY.x + CubicY.y, CubicY.z + CubicY.w);
    vec4 Offset    = Coord + vec4(CubicX.y, CubicX.w, CubicY.y, CubicY.w) / WeightSum;

    vec4 Sample0 = texture(Tex, vec2(Offset.x, Offset.z) / iResolution);
    vec4 Sample1 = texture(Tex, vec2(Offset.y, Offset.z) / iResolution);
    vec4 Sample2 = texture(Tex, vec2(Offset.x, Offset.w) / iResolution);
    vec4 Sample3 = texture(Tex, vec2(Offset.y, Offset.w) / iResolution);

    float SumX = WeightSum.x / (WeightSum.x + WeightSum.y);
    float SumY = WeightSum.z / (WeightSum.z + WeightSum.w);

    return mix(mix(Sample3, Sample2, SumX), mix(Sample1, Sample0, SumX), SumY);
}

vec3 ColorFetch(vec2 TexCoord)
{
    return texture(iBloomTexs[0], TexCoord).rgb;
}

vec3 BloomFetch(vec2 TexCoord)
{
    return BicubicTexture(iBloomTexs[1], TexCoord).rgb;
}

vec3 Grab(vec2 TexCoord, float Octave, vec2 Offset)
{
    float Scale = exp2(Octave);

    TexCoord /= Scale;
    TexCoord -= Offset;

    return BloomFetch(TexCoord);
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

vec3 GetBloom(vec2 TexCoord)
{
    vec3 Bloom = vec3(0.0);

    // Reconstruct bloom from multiple blurred images
    Bloom += Grab(TexCoord, 1.0, vec2(CalcOffset(0.0))) * 1.0;
    Bloom += Grab(TexCoord, 2.0, vec2(CalcOffset(1.0))) * 1.5;
    Bloom += Grab(TexCoord, 3.0, vec2(CalcOffset(2.0))) * 1.0;
    Bloom += Grab(TexCoord, 4.0, vec2(CalcOffset(3.0))) * 1.5;
    Bloom += Grab(TexCoord, 5.0, vec2(CalcOffset(4.0))) * 1.8;
    Bloom += Grab(TexCoord, 6.0, vec2(CalcOffset(5.0))) * 1.0;
    Bloom += Grab(TexCoord, 7.0, vec2(CalcOffset(6.0))) * 1.0;
    Bloom += Grab(TexCoord, 8.0, vec2(CalcOffset(7.0))) * 1.0;

    return Bloom;
}

void main()
{
    vec2 FragUv = gl_FragCoord.xy / iResolution;
    vec3 Color  = ColorFetch(FragUv);
    Color += GetBloom(FragUv) * 0.08;

    // Tonemapping and color grading
    Color = pow(Color, vec3(1.5));
    Color = Color / (1.0 + Color);
    Color = pow(Color, vec3(1.0 / 1.5));

    Color = mix(Color, Color * Color * (3.0 - 2.0 * Color), vec3(1.0));
    Color = pow(Color, vec3(1.3, 1.20, 1.0));

    Color = Saturate(Color * 1.01);

    Color = pow(Color, vec3(0.7 / 2.2));

    FragColor = vec4(Color, 1.0);
}
