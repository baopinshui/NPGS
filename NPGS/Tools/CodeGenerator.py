def generate_structured_binding(count):
    # 生成变量名序列 Ax-Zx, Ay-Zy, Az-Zz, Aw-Zw
    suffixes = ['x', 'y', 'z', 'w']
    all_vars = []
    for suffix in suffixes:
        all_vars.extend(f"{chr(65+i)}{suffix}" for i in range(26))
    
    # 取需要数量的变量
    vars_needed = all_vars[:count]
    
    # 每26个变量一行，确保每行末尾都有逗号
    lines = []
    for i in range(0, len(vars_needed), 26):
        chunk = vars_needed[i:i+26]
        line = ", ".join(chunk)
        # 如果不是最后一组变量，确保添加逗号
        if i + 26 < len(vars_needed):
            line += ","
        lines.append(line)
    
    return "\n        ".join(lines)

def generate_workaround_cast(count):
    # 生成WorkAroundCast调用序列
    vars_needed = []
    suffixes = ['x', 'y', 'z', 'w']
    for suffix in suffixes:
        vars_needed.extend(f"{chr(65+i)}{suffix}" for i in range(26))
    vars_needed = vars_needed[:count]
    
    # 生成WorkAroundCast调用，每两个一行
    cast_lines = []
    for i in range(0, len(vars_needed), 2):
        if i + 1 < len(vars_needed):
            line = (f"WorkAroundCast<StructType, decltype({vars_needed[i]})>({vars_needed[i]}), "
                   f"WorkAroundCast<StructType, decltype({vars_needed[i+1]})>({vars_needed[i+1]})")
        else:
            line = f"WorkAroundCast<StructType, decltype({vars_needed[i]})>({vars_needed[i]})"
        cast_lines.append(line)
    
    return ",\n                    ".join(cast_lines)

def generate_tie_as_tuple(count):
    return f"""template <class StructType>
constexpr auto TieAsTuple(StructType& Value, SizeT<{count}>) noexcept
{{
    auto& [
        {generate_structured_binding(count)}
    ] = Value;

    return std::tie({generate_workaround_cast(count)});
}}
"""

# 生成所有重载函数
with open('generated_tie_as_tuple.hpp', 'w', encoding='utf-8') as f:
    for i in range(1, 105):
        f.write(generate_tie_as_tuple(i))
        f.write('\n')

print("代码生成完成，已保存到 generated_tie_as_tuple.hpp")
