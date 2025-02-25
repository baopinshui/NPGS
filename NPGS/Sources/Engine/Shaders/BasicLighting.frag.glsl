#version 450
#pragma shader_stage(fragment)

layout(location = 0) in  vec2 TexCoordFromVert;
layout(location = 1) in  vec3 NormalFromVert;
layout(location = 2) in  vec3 FragPosFromVert;
layout(location = 0) out vec4 FragColor;

struct FMaterial
{
    float Shininess;
};

struct FLight
{
    vec3 Position;
    vec3 Ambient;
    vec3 Diffuse;
    vec3 Specular;
};

layout(set = 0, binding = 1) uniform LightMaterial
{
    FMaterial iMaterial;
    FLight    iLight;
    vec3      iViewPos;
};

layout(set = 1, binding = 0) uniform sampler iSampler;
layout(set = 1, binding = 1) uniform texture2D iDiffuseTex;
layout(set = 1, binding = 2) uniform texture2D iSpecularTex;

void main()
{
#ifdef LAMP_BOX
    FragColor  = vec4(1.0, 1.0, 1.0, 1.0);
#else
    vec3 AmbientColor = iLight.Ambient * texture(sampler2D(iDiffuseTex, iSampler), TexCoordFromVert).rgb;

    vec3  Normal       = normalize(NormalFromVert);
    vec3  LightDir     = normalize(iLight.Position - FragPosFromVert);
    float DiffuseCoef  = max(dot(Normal, LightDir), 0.0);
    vec3  DiffuseColor = iLight.Diffuse * DiffuseCoef * texture(sampler2D(iDiffuseTex, iSampler), TexCoordFromVert).rgb;

    vec3  ViewDir       = normalize(iViewPos - FragPosFromVert);
    vec3  ReflectDir    = reflect(-LightDir, Normal);
    float SpecularCoef  = pow(max(dot(ViewDir, ReflectDir), 0.0), iMaterial.Shininess);
    vec3  SpecularColor = iLight.Specular * SpecularCoef * texture(sampler2D(iSpecularTex, iSampler), TexCoordFromVert).rgb;

    vec3 Result = AmbientColor + DiffuseColor + SpecularColor;
    FragColor = vec4(Result, 1.0);
#endif
}
