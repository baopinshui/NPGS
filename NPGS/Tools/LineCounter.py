import os
import re

def count_lines(file_path):
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
    return total, logical

def main():
    source_dir = os.path.join(os.getcwd(), '../Sources')
    extensions = ('.cpp', '.h', '.hpp', '.inl')
    total_all = 0
    total_logical = 0
    for root, dirs, files in os.walk(source_dir):
        for file in files:
            if file.endswith(extensions):
                file_path = os.path.join(root, file)
                t, l = count_lines(file_path)
                total_all += t
                total_logical += l
    print(f"总行数（包括空行和注释）: {total_all}")
    print(f"逻辑行数（去除空行和注释）: {total_logical}")

if __name__ == "__main__":
    main()