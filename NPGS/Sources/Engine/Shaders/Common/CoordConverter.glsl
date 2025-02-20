#ifndef COORDCONVERTER_GLSL_
#define COORDCONVERTER_GLSL_

vec3 FragUvToDir(vec2 FragUv, float Fov, vec2 NdcResolution)
{
    return normalize(vec3(Fov * (2.0 * FragUv.x - 1.0),
                          Fov * (2.0 * FragUv.y - 1.0) * NdcResolution.y / NdcResolution.x,
                          -1.0));
}

vec2 PosToNdc(vec4 Pos, vec2 NdcResolution)
{
    return vec2(-Pos.x / Pos.z, -Pos.y / Pos.z * NdcResolution.x / NdcResolution.y);
}

vec2 DirToNdc(vec3 Dir, vec2 NdcResolution)
{
    return vec2(-Dir.x / Dir.z, -Dir.y / Dir.z * NdcResolution.x / NdcResolution.y);
}

vec2 DirToFragUv(vec3 Dir, vec2 NdcResolution)
{
    return vec2(0.5 - 0.5 * Dir.x / Dir.z, 0.5 - 0.5 * Dir.y / Dir.z * NdcResolution.x / NdcResolution.y);
}

vec2 PosToFragUv(vec4 Pos, vec2 NdcResolution)
{
    return vec2(0.5 - 0.5 * Pos.x / Pos.z, 0.5 - 0.5 * Pos.y / Pos.z * NdcResolution.x / NdcResolution.y);
}

#endif // !COORDCONVERTER_GLSL_
