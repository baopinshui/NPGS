#ifndef RANDOM_HASH_GLSL_
#define RANDOM_HASH_GLSL_

// 1D -> 1D hash
float Hash11(float Base, float FractMultipler, float BaseAddend)
{
    Base = fract(Base * FractMultipler);
    Base *= Base + BaseAddend;
    Base *= Base + Base;
    return fract(Base);
}

float Hash11(float Base)
{
    return Hash11(Base, 0.1031, 33.33);
}

// 2D -> 1D hash
float Hash21(vec2 Base, float FractMultipler, float BaseAddend)
{
    vec3 Base3 = fract(vec3(Base.xyx) * FractMultipler);
    Base3 += dot(Base3, Base3.yzx + BaseAddend);
    return fract((Base3.x + Base3.y) * Base3.z);
}

float Hash21(vec2 Base)
{
    return Hash21(Base, 0.1031, 33.33);
}

// 2D -> 2D hash
vec2 Hash22(vec2 Base, vec3 FractMultiplers, float BaseAddend)
{
    vec3 Base3 = fract(vec3(Base.xyx) * FractMultiplers);
    Base3 += dot(Base3, Base3.yzx + BaseAddend);
    return fract((Base3.xx + Base3.yz) * Base3.zy);
}

vec2 Hash22(vec2 Base)
{
    return Hash22(Base, vec3(0.1031, 0.1030, 0.0973), 33.33);
}

// 3D -> 1D hash
float Hash31(vec3 Base, vec3 FractMultiplers, float BaseAddend)
{
    vec3 Base3 = fract(Base * FractMultiplers);
    Base3 += dot(Base3, Base3.yxz + BaseAddend);
    return fract((Base3.x + Base3.y) * Base3.z);
}

float Hash31(vec3 Base)
{
    return Hash31(Base, vec3(0.1031, 0.1030, 0.0973), 33.33);
}

// 3D -> 3D hash
vec3 Hash33(vec3 Base, vec3 FractMultiplers, float BaseAddend)
{
    Base = fract(Base * FractMultiplers);
    Base += dot(Base, Base.yxz + BaseAddend);
    return fract((Base.xxy + Base.yxx) * Base.zyx);
}

vec3 Hash33(vec3 Base)
{
    return Hash33(Base, vec3(0.1031, 0.1030, 0.0973), 33.33);
}

// 4D -> 1D hash
float Hash41(vec4 Base, vec4 FractMultiplers, float BaseAddend)
{
    vec4 Base4 = fract(Base * FractMultiplers);
    Base4 += dot(Base4, Base4.wzxy + BaseAddend);
    return fract((Base4.x + Base4.y + Base4.z) * Base4.w);
}

float Hash41(vec4 Base)
{
    return Hash41(Base, vec4(0.1031, 0.1030, 0.0973, 0.1099), 33.33);
}

// 4D -> 4D hash
vec4 Hash44(vec4 Base, vec4 FractMultiplers, float BaseAddend)
{
    Base = fract(Base * FractMultiplers);
    Base += dot(Base, Base.wzxy + BaseAddend);
    return fract((Base.xxyz + Base.yzzw + Base.zywx + Base.wxxy) * Base.wzxy);
}

vec4 Hash44(vec4 Base)
{
    return Hash44(Base, vec4(0.1031, 0.1030, 0.0973, 0.1099), 33.33);
}

#endif // !RANDOM_HASH_GLSL_
