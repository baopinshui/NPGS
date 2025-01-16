#version 460 core

layout(location = 0) out vec4 FragColor;

void main()
{
    vec3 Color = vec3(0.0);
    // 1. 使用不确定的值,防止编译器优化
    float t = gl_FragCoord.x * gl_FragCoord.y;
    
    // 2. 在循环中使用该不确定值
    for (int i = 0; i < 100000; i++) {
        Color += vec3(sin(t * float(i)));
        
        // 3. 添加依赖关系,防止并行优化
        t = fract(t * Color.x);
    }
    
    // 4. 确保计算结果被使用
    FragColor = vec4(Color, 1.0);
}
