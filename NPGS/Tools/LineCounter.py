import os

def count_lines(file_path):
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
        total = len(lines)
        logical = 0
        in_block_comment = False
        for line in lines:
            stripped = line.strip()
            if not stripped:
                continue
            if in_block_comment:
                if '*/' in stripped:
                    in_block_comment = False
                continue
            if stripped.startswith('//'):
                continue
            if stripped.startswith('/*'):
                in_block_comment = True
                continue
            logical += 1
        print(f"文件 {os.path.basename(file_path)}: 总行数 = {total}, 逻辑行数 = {logical}")
        return total, logical
    except Exception as e:
        print(f"处理文件 {file_path} 时出错: {str(e)}")
        return 0, 0

def main():
    # 使用脚本所在目录作为基准点
    script_dir = os.path.dirname(os.path.abspath(__file__))
    source_dir = os.path.abspath(os.path.join(script_dir, '../Sources'))
    
    # 移动打印语句到变量定义之后
    print(f"脚本目录: {script_dir}")
    print(f"源代码目录: {source_dir}")
    
    if not os.path.exists(source_dir):
        print(f"错误：源代码目录不存在: {source_dir}")
        return
        
    extensions = ('.cpp', '.h', '.hpp', '.inl')
    total_all = 0
    total_logical = 0
    
    print(f"开始扫描目录: {source_dir}")
    
    for root, dirs, files in os.walk(source_dir):
        for file in files:
            if file.lower().endswith(extensions):
                file_path = os.path.join(root, file)
                t, l = count_lines(file_path)
                total_all += t
                total_logical += l
    
    print("\n统计结果:")
    print(f"总行数（包括空行和注释）: {total_all}")
    print(f"逻辑行数（去除空行和注释）: {total_logical}")

if __name__ == "__main__":
    main()